// Copyright 2018 Google LLC
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


#include "stratum/hal/lib/common/constants.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/macros.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
//FIXME remove when onlp_wrapper.h is stable
//#include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
#include "stratum/hal/lib/phal/onlp/onlphal.h"

DEFINE_int32(max_num_transceiver_writers, 2,
             "Maximum number of channel writers for transceiver events.");

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using TransceiverEvent = PhalInterface::TransceiverEvent;
using TransceiverEventWriter = PhalInterface::TransceiverEventWriter;

Onlphal* Onlphal::singleton_ = nullptr;
#ifdef ABSL_KCONSTINIT //FIXME remove when kConstInit is upstreamed
ABSL_CONST_INIT absl::Mutex Onlphal::init_lock_(absl::kConstInit);
#else
absl::Mutex Onlphal::init_lock_;
#endif


::util::Status OnlphalSfpEventCallback::HandleSfpStatusChange(
    const OidInfo& oid_info) {

  // Format TransceiverEvent
  TransceiverEvent event;
  event.slot = 0;
  event.port = oid_info.GetId();
  event.state = oid_info.GetHardwareState();

  // Write the event to each writer
  ::util::Status result = onlphal_->WriteTransceiverEvent(event);
  return result;
}

Onlphal::Onlphal() : 
    onlp_interface_(nullptr),
    onlp_event_handler_(nullptr),
    sfp_event_callback_(nullptr) {
}

Onlphal::~Onlphal() {}

::util::Status Onlphal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    // TODO: Implement this function.

    // Create the OnlpWrapper object
    RETURN_IF_ERROR(InitializeOnlpInterface());

    //Creates Data Source objects.
    // TODO: move the logic to OnlpConfigurator later.
    RETURN_IF_ERROR(InitializeOnlpOids());

    // Create the OnlpEventHandler object with given OnlpWrapper
    RETURN_IF_ERROR(InitializeOnlpEventHandler());

    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status Onlphal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status Onlphal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  // TODO: add clean up code

  initialized_ = false;

  return ::util::OkStatus();
}

::util::StatusOr<int> Onlphal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {

  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  absl::WriterMutexLock l(&config_lock_);
  CHECK_RETURN_IF_FALSE(transceiver_event_writers_.size() <
                        static_cast<size_t>(FLAGS_max_num_transceiver_writers))
      << "Can only support " << FLAGS_max_num_transceiver_writers
      << " transceiver event Writers.";

  // Find the next available ID for the Writer.
  int next_id = kInvalidWriterId;
  for (int id = 1; id <= static_cast<int>(transceiver_event_writers_.size()) + 1;
       ++id) {
    auto it = std::find_if(
        transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
        [id](const TransceiverEventWriter& w) { return w.id == id; });
    if (it == transceiver_event_writers_.end()) {
      // This id is free. Pick it up.
      next_id = id;
      break;
    }
  }
  CHECK_RETURN_IF_FALSE(next_id != kInvalidWriterId)
      << "Could not find a new ID for the Writer. next_id=" << next_id << ".";

  transceiver_event_writers_.insert({std::move(writer), priority, next_id});

  // Note: register callback only after writer registered. Only register
  //       callback once.
  if (sfp_event_callback_ == nullptr) {
    // Create OnlpSfpEventCallback
    std::unique_ptr<OnlphalSfpEventCallback> 
        callback(new OnlphalSfpEventCallback());
    sfp_event_callback_ = std::move(callback);
    sfp_event_callback_->onlphal_ = this;
 
    // Register OnlpSfpEventCallback
    ::util::Status result = 
      onlp_event_handler_->RegisterSfpEventCallback(sfp_event_callback_.get());
    CHECK_RETURN_IF_FALSE(result.ok())
        << "Failed to register SFP event callback.";
  }

  return next_id;
}

::util::Status Onlphal::UnregisterTransceiverEventWriter(int id) {

  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  absl::WriterMutexLock l(&config_lock_);
  auto it = std::find_if(
      transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
      [id](const TransceiverEventWriter& h) { return h.id == id; });
  CHECK_RETURN_IF_FALSE(it != transceiver_event_writers_.end())
      << "Could not find a transceiver event Writer with ID " << id << ".";

  transceiver_event_writers_.erase(it);

  // Unregister OnlpSfpEventCallback if no more writer registered
  if (transceiver_event_writers_.size() == 0 && 
      sfp_event_callback_ != nullptr) {
    ::util::Status result = onlp_event_handler_->UnregisterSfpEventCallback(
        sfp_event_callback_.get());
    CHECK_RETURN_IF_FALSE(result.ok())
        << "Failed to unregister SFP event callback.";
    sfp_event_callback_ = nullptr;
  }

  return ::util::OkStatus();
}

::util::Status Onlphal::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {

  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  //absl::WriterMutexLock l(&config_lock_);

  //Get slot port pair to lookup sfpdatasource.
  const std::pair<int, int>& slot_port_pair = std::make_pair(slot, port);

  std::shared_ptr<OnlpSfpDataSource> sfp_src = slot_port_to_sfp_data_[slot_port_pair];
  //Update sfp datasource values.
  sfp_src->UpdateValuesUnsafelyWithoutCacheOrLock();
  
  ManagedAttribute *sfptype_attrib = sfp_src->GetSfpType();



  ASSIGN_OR_RETURN(
        auto sfptype_value,
        sfptype_attrib->ReadValue<const google::protobuf::EnumValueDescriptor*>());

  SfpType sfval = static_cast<SfpType>(sfptype_value->index());
  //Need to map SfpType to PhysicalPortType
  PhysicalPortType actual_val;
  switch(sfval) {

	case SFP_TYPE_SFP:
		actual_val = PHYSICAL_PORT_TYPE_SFP_CAGE;
		break;
	case SFP_TYPE_QSFP:
		actual_val = PHYSICAL_PORT_TYPE_QSFP_CAGE;
		break;
	default:
		RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid sfptype. ";

  }
  fp_port_info->set_physical_port_type(actual_val);

  ManagedAttribute *mediatype_attrib = sfp_src->GetSfpMediaType();
  ASSIGN_OR_RETURN(
        auto mediatype_value,
        mediatype_attrib->ReadValue<const google::protobuf::EnumValueDescriptor*>());

  MediaType mediat_val = static_cast<MediaType>(mediatype_value->index());
  fp_port_info->set_media_type(mediat_val);

  ManagedAttribute *vendor_attrib = sfp_src->GetSfpVendor();
  ASSIGN_OR_RETURN(
        auto vendor_value,
        vendor_attrib->ReadValue<std::string>());
  fp_port_info->set_vendor_name(vendor_value);
  
   ManagedAttribute *model_attrib = sfp_src->GetSfpModel();
  ASSIGN_OR_RETURN(
        auto model_value,
        model_attrib->ReadValue<std::string>());
  fp_port_info->set_part_number(model_value);

  ManagedAttribute *serial_attrib = sfp_src->GetSfpSerialNumber();
  ASSIGN_OR_RETURN(
        auto serial_value,
        serial_attrib->ReadValue<std::string>());
  fp_port_info->set_serial_number(serial_value);

  return ::util::OkStatus();
}

Onlphal* Onlphal::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
 
  if (!singleton_) {
    singleton_ = new Onlphal();
  }

  return singleton_;
}

::util::Status Onlphal::WriteTransceiverEvent(const TransceiverEvent& event) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  absl::WriterMutexLock l(&config_lock_);

  std::multiset<TransceiverEventWriter, TransceiverEventWriterComp>::iterator
    it;  
  for (it = transceiver_event_writers_.begin(); 
       it != transceiver_event_writers_.end();
       ++ it) {
    /*
    LOG(INFO) << "In Onlphal::WriteTransceiverEvent, Writer, id=" << it->id
              << ", priority=" << it->priority;
    LOG(INFO) << "Onlphal::WriteTransceiverEvent, Event, slot=" << event.slot
              << ", port=" << event.port << ", state=" << event.state;
    */
    it->writer->Write(event, absl::InfiniteDuration());
  }

  return ::util::OkStatus();
}

::util::Status Onlphal::SetPortLedState(int slot, int port, int channel,
                                        LedColor color, LedState state) {
  // TODO: Implement this.
  return ::util::OkStatus();
}

::util::Status Onlphal::InitializeOnlpInterface() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "InitializeOnlpInterface() can be called only before the class is "
           << "initialized";
  }

  //absl::WriterMutexLock l(&config_lock_);

  // Create the OnlpInterface object
  ::util::StatusOr<std::unique_ptr<OnlpInterface>> resInterface =
      OnlpWrapper::Make();
  if (resInterface.ok()) {
    onlp_interface_ = resInterface.ConsumeValueOrDie();
    return ::util::OkStatus();
  } else {
    LOG(ERROR) << resInterface.status();
    return resInterface.status();
  }
}

::util::Status Onlphal::InitializeOnlpEventHandler() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "InitializeOnlpEventHandler() can be called only before the "
           << "class is initialized";
  }

  //absl::WriterMutexLock l(&config_lock_);

  // Create the OnlpEventHandler object 
  ::util::StatusOr<std::unique_ptr<OnlpEventHandler>> resultEvHandler =
      OnlpEventHandler::Make(onlp_interface_.get());
  if (resultEvHandler.ok()) {
    onlp_event_handler_ = resultEvHandler.ConsumeValueOrDie();
    return ::util::OkStatus();
  } else {
    LOG(ERROR) << resultEvHandler.status();
    return resultEvHandler.status();
  }
}

::util::Status Onlphal::InitializeOnlpOids() {
	//Get list of sfps.
	::util::StatusOr<std::vector <OnlpOid>> result = 
		onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_SFP);
	if (!result.ok()) {
		LOG(ERROR) << result.status();
		return result.status();
	}
	
	std::vector <OnlpOid> OnlpOids = 
		result.ConsumeValueOrDie();

	//Assuming slot is always zero.. 
	for(unsigned int i = 0; i < OnlpOids.size(); i++) {

		//Adding 1, because port numbering starts from 1.

 		const std::pair<int, int>& slot_port_pair = std::make_pair(0, i+1);
		::util::StatusOr<std::shared_ptr<OnlpSfpDataSource>> result = 
			OnlpSfpDataSource::Make(OnlpOids[i], onlp_interface_.get(), NULL);

		if (!result.ok()) {
			LOG(ERROR) << result.status();
			return result.status();
		}

		std::shared_ptr<OnlpSfpDataSource> sfp_data_src = result.ConsumeValueOrDie();

		
		slot_port_to_sfp_data_[slot_port_pair] = sfp_data_src;
	}
	return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

