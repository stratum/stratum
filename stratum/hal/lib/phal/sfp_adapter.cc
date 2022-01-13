// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/sfp_adapter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/utils.h"

DEFINE_int32(max_num_transceiver_writers, 2,
             "Maximum number of channel writers for transceiver events.");

namespace stratum {
namespace hal {
namespace phal {

SfpAdapter::SfpAdapter(AttributeDatabaseInterface* attribute_db_interface)
    : Adapter(ABSL_DIE_IF_NULL(attribute_db_interface)) {}

SfpAdapter::~SfpAdapter() {
  if (sfp_reader_thread_.joinable()) {
    channel_->Close();
    sfp_reader_thread_.join();
  }
}

::util::Status SfpAdapter::GetFrontPanelPortInfo(
    int card_id, int port_id, FrontPanelPortInfo* fp_port_info) {
  if (card_id <= 0 || port_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  std::vector<Path> paths = {
      // PhalDb uses 0-based index, while BcmPort::slot is 1-based.
      {PathEntry("cards", card_id - 1), PathEntry("ports", port_id - 1),
       PathEntry("transceiver", -1, false, false, true)}};

  // Get PhalDB entry for this port
  ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  // Get card
  CHECK_RETURN_IF_FALSE(phaldb->cards_size() > card_id - 1)
      << "cards[" << card_id << "]"
      << " not found!";

  auto card = phaldb->cards(card_id - 1);

  // Get port
  CHECK_RETURN_IF_FALSE(card.ports_size() > port_id - 1)
      << "cards[" << card_id << "]/ports[" << port_id << "]"
      << " not found!";

  auto phal_port = card.ports(port_id - 1);

  // Get the SFP (transceiver)
  if (!phal_port.has_transceiver()) {
    return MAKE_ERROR() << "cards[" << card_id << "]/ports[" << port_id
                        << "] has no transceiver";
  }
  auto sfp = phal_port.transceiver();

  // Convert HW state and don't continue if not present
  fp_port_info->set_hw_state(sfp.hardware_state());
  if (fp_port_info->hw_state() == HW_STATE_NOT_PRESENT) {
    return ::util::OkStatus();
  }

  // Need to map connector_type to PhysicalPortType
  PhysicalPortType actual_val;
  switch (sfp.connector_type()) {
    case SFP_TYPE_SFP28:
    case SFP_TYPE_SFP:
      actual_val = PHYSICAL_PORT_TYPE_SFP_CAGE;
      break;
    case SFP_TYPE_QSFP_PLUS:
    case SFP_TYPE_QSFP:
    case SFP_TYPE_QSFP28:
      actual_val = PHYSICAL_PORT_TYPE_QSFP_CAGE;
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid sfptype. ";
  }
  fp_port_info->set_physical_port_type(actual_val);

  fp_port_info->set_media_type(sfp.media_type());

  if (sfp.has_info()) {
    fp_port_info->set_vendor_name(sfp.info().mfg_name());
    fp_port_info->set_part_number(sfp.info().part_no());
    fp_port_info->set_serial_number(sfp.info().serial_no());
  }

  return ::util::OkStatus();
}

::util::StatusOr<int> SfpAdapter::RegisterSfpEventSubscriber(
    std::unique_ptr<ChannelWriter<PhalInterface::TransceiverEvent>> writer,
    int priority) {
  int id;
  {
    absl::WriterMutexLock l(&subscribers_lock_);
    CHECK_RETURN_IF_FALSE(subscribers_.size() <
                          FLAGS_max_num_transceiver_writers)
        << "Can only support " << FLAGS_max_num_transceiver_writers
        << " transceiver event Writers.";
    // Setup db subscription once on first subscriber.
    // FIXME: move to SfpAdapter::Make() factory?
    if (subscribers_.empty()) {
      CHECK(SetupSfpDatabaseSubscriptions().ok());
    }
    // FIXME: this only works because we never delete elements.
    id = subscribers_.size() + 1;
    PhalInterface::TransceiverEventWriter subscriber{std::move(writer),
                                                     priority, id};
    subscribers_.emplace_back(std::move(subscriber));
    std::sort(subscribers_.begin(), subscribers_.end(),
              PhalInterface::TransceiverEventWriterComp{});
  }
  OneShotUpdate();
  return id;
}

::util::Status SfpAdapter::UnregisterSfpEventSubscriber(int id) {
  absl::WriterMutexLock l(&subscribers_lock_);
  CHECK_RETURN_IF_FALSE(id <= subscribers_.size())
      << "Invalid subscriber id : " << id;

  auto it =
      std::find_if(subscribers_.begin(), subscribers_.end(),
                   [&id](const PhalInterface::TransceiverEventWriter& sub) {
                     return sub.id == id;
                   });
  if (it == subscribers_.end()) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "No subscriber with id " << id << " exists.";
  }

  // FIXME: either use a map or delete elements. The id generation has then to
  // be fixed too.
  it->writer = nullptr;
  it->id = -1;
  it->priority = 0;

  std::sort(subscribers_.begin(), subscribers_.end(),
            PhalInterface::TransceiverEventWriterComp{});

  return ::util::OkStatus();
}

::util::Status SfpAdapter::OneShotUpdate() {
  ASSIGN_OR_RETURN(auto phal_db_update, Get({kAllTransceiversPath}));

  for (int slot = 0; slot < phal_db_update->cards_size(); ++slot) {
    auto card = phal_db_update->cards(slot);
    for (int port_idx = 0; port_idx < card.ports_size(); ++port_idx) {
      auto port = card.ports(port_idx);
      auto state = port.transceiver().hardware_state();
      // The one shot update only includes present ports.
      if (state != HwState::HW_STATE_PRESENT) {
        continue;
      }
      absl::WriterMutexLock l(&subscribers_lock_);
      // Notify all subscribers.
      VLOG(2) << "One shot Transceiver_event { slot: " << slot + 1
              << ", port: " << port_idx + 1
              << ", state: " << HwState_Name(state) << " } to "
              << subscribers_.size() << " subscribers";
      PhalInterface::TransceiverEvent event{slot + 1, port_idx + 1, state};
      for (auto& subscriber : subscribers_) {
        // Short timeout and no error handling.
        if (subscriber.writer) {
          subscriber.writer->Write(event, absl::Milliseconds(10));
        }
      }
    }
  }
  return ::util::OkStatus();
}

::util::Status SfpAdapter::TransceiverEventReaderThreadFunc(
    std::unique_ptr<ChannelReader<PhalDB>> reader) {
  // Read initial sfp states.
  PhalDB last_phal_db_update;
  CHECK(reader->Read(&last_phal_db_update, absl::InfiniteDuration()).ok());
  PhalDB phal_db_update;
  while (true) {
    // Read until channel is closed on shutdown.
    auto status = reader->Read(&phal_db_update, absl::InfiniteDuration());
    if (status.error_code() == ERR_CANCELLED) {
      return ::util::OkStatus();
    }
    RETURN_IF_ERROR(status);

    VLOG(2) << "SfpAdapter: attribute Db transceiver update: "
            << phal_db_update.ShortDebugString();
    // We need the indices for the TransceiverEvent event.
    for (int slot = 0; slot < phal_db_update.cards_size(); ++slot) {
      auto card = phal_db_update.cards(slot);
      for (int port_idx = 0; port_idx < card.ports_size(); ++port_idx) {
        auto port = card.ports(port_idx);
        auto state = port.transceiver().hardware_state();
        auto old_state = last_phal_db_update.cards(slot)
                             .ports(port_idx)
                             .transceiver()
                             .hardware_state();
        // Skip empty ports in update
        if (state == HwState::HW_STATE_UNKNOWN || state == old_state) {
          continue;
        }
        absl::WriterMutexLock l(&subscribers_lock_);
        // Notify all subscribers.
        VLOG(2) << "Sending transceiver_event { slot: " << slot + 1
                << ", port: " << port_idx + 1
                << ", state: " << HwState_Name(state) << " } to "
                << subscribers_.size() << " subscribers";
        PhalInterface::TransceiverEvent event{slot + 1, port_idx + 1, state};
        for (auto& subscriber : subscribers_) {
          // Short timeout and no error handling.
          if (subscriber.writer) {
            subscriber.writer->Write(event, absl::Milliseconds(10));
          }
        }
      }
    }
    last_phal_db_update = phal_db_update;
  }
}

::util::Status SfpAdapter::SetupSfpDatabaseSubscriptions() {
  if (sfp_reader_thread_.joinable() || query_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Database subscription already created before.";
  }

  channel_ = Channel<PhalDB>::Create(kDefaultChannelDepth);
  auto reader = ChannelReader<PhalDB>::Create(channel_);
  auto writer = ChannelWriter<PhalDB>::Create(channel_);
  ASSIGN_OR_RETURN(query_, Subscribe({kAllTransceiversPath}, std::move(writer),
                                     absl::Seconds(1)));

  std::thread t(&SfpAdapter::TransceiverEventReaderThreadFunc, this,
                std::move(reader));
  sfp_reader_thread_ = std::move(t);

  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
