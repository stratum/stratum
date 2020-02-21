// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/tai/taish_client.h"

#include "absl/strings/str_split.h"

namespace stratum {
namespace hal {
namespace phal {

TaishClient::TaishClient(std::shared_ptr<grpc::Channel> channel)
    : taish_(taish::TAI::NewStub(channel)) {
  // get modules from TAI
  GetListModule();
}

/*!
 * \brief TaishClient::GetListAttrs method gets list of modules from TAI lib
 */
void TaishClient::GetListModule() {
  grpc::ClientContext context;

  taish::ListModuleRequest request;
  taish::ListModuleResponse responce;

  auto list_reader = taish_->ListModule(&context, request);

  while (list_reader->Read(&responce)) {
    const auto& r_module = responce.module();
    auto module = Module(r_module.oid(), r_module.location());

    for (const auto& netif : r_module.netifs())
      module.AddNetif(netif.oid(), netif.index());

    for (const auto& hostif : r_module.hostifs())
      module.AddNetif(hostif.oid(), hostif.index());

    modules_.push_back(module);
  }

  list_reader->Finish();
}

/*!
 * \brief TaishClient::GetValue method gets value from TAI by \param
 * module_location, \param network_index and \param attr_name
 */
util::StatusOr<std::string> TaishClient::GetValue(
    const std::string& module_location, int network_index,
    const std::string& attr_name) {
  grpc::ClientContext context;
  taish::GetAttributeRequest request;

  auto it = std::find_if(modules_.begin(), modules_.end(), [module_location](Module& m) {
    return m.location_ == module_location;
  });
  CHECK_RETURN_IF_FALSE(it != modules_.end());
  Module module = *it;

  CHECK_RETURN_IF_FALSE(network_index < module.netifs.size());
  Netif netif = module.netifs.at(network_index);

  request.set_oid(netif.object_id_);
  request.mutable_serialize_option()->set_value_only(true);
  request.mutable_serialize_option()->set_human(true);
  request.mutable_serialize_option()->set_json(false);

  auto metadata = GetMetadata(netif.object_type, attr_name);
  request.mutable_attribute()->set_attr_id(metadata.attr_id());

  taish::GetAttributeResponse response;

  auto status = taish_->GetAttribute(&context, request, &response);
  CHECK_RETURN_IF_FALSE(status.ok()) << status.error_message();

  return response.attribute().value();
}

/*!
 * \brief TaishClient::SetValue method sets given \param value to TAI library by
 * \param module_location, \param network_index and \param attr_name
 */
util::Status TaishClient::SetValue(const std::string& module_location,
                                   int network_index,
                                   const std::string& attr_name,
                                   const std::string& value) {
  grpc::ClientContext context;
  taish::SetAttributeRequest request;

  Module module;
  for (const auto& mod : modules_)
    if (mod.location_ == module_location) module = mod;

  Netif netif;
  if (network_index < module.netifs.size())
    netif = module.netifs.at(network_index);

  request.set_oid(netif.object_id_);

  auto metadata = GetMetadata(netif.object_type, attr_name);
  request.mutable_attribute()->set_attr_id(metadata.attr_id());
  request.mutable_attribute()->set_value(value);

  request.mutable_serialize_option()->set_json(false);
  request.mutable_serialize_option()->set_human(true);
  request.mutable_serialize_option()->set_value_only(true);
  taish::SetAttributeResponse response;

  auto status = taish_->SetAttribute(&context, request, &response);
  if (!status.ok()) RETURN_ERROR() << "Unable to set attribute";

  RETURN_OK();
}

/*!
 * \brief TaishClient::GetMetadata method \return metadata for concrete \param
 * object_type and \param attr_name
 */
::taish::AttributeMetadata TaishClient::GetMetadata(
    taish::TAIObjectType object_type, const std::string& attr_name) {
  ::taish::AttributeMetadata attribute;
  for (const auto& attr : ListMetadata(object_type)) {
    if (attr.name() == attr_name) return attr;
  }
  return {};
}

/*!
 * \brief TaishClient::ListMetadata method \return all metadata for given \param
 * object_type
 */
std::vector<::taish::AttributeMetadata> TaishClient::ListMetadata(
    taish::TAIObjectType object_type) {
  grpc::ClientContext context;

  taish::ListAttributeMetadataRequest request;
  request.set_object_type(object_type);

  auto list_reader = taish_->ListAttributeMetadata(&context, request);

  taish::ListAttributeMetadataResponse responce;

  std::vector<::taish::AttributeMetadata> attrs_list;
  while (list_reader->Read(&responce)) {
    attrs_list.push_back(responce.metadata());
  }

  return attrs_list;
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
