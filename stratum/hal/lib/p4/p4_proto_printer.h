// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_P4_P4_PROTO_PRINTER_H_
#define STRATUM_HAL_LIB_P4_P4_PROTO_PRINTER_H_

#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
namespace stratum {
namespace hal {
namespace {

class MyMessagePrinter : public ::google::protobuf::TextFormat::MessagePrinter {
 public:
  MyMessagePrinter() {}
  ~MyMessagePrinter() override {}

  void Print(const ::google::protobuf::Message& message, bool single_line_mode,
             ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    LOG(WARNING) << "foo";
    // LOG(WARNING) << "message: " << message.ShortDebugString();
    // LOG(WARNING) << "single_line_mode: " << single_line_mode;
    ::google::protobuf::TextFormat::MessagePrinter::Print(
        message, single_line_mode, generator);
    return;
  }
};

class ActionPrettyPrinter
    : public ::google::protobuf::TextFormat::MessagePrinter {
 public:
  ActionPrettyPrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)) {}

  void Print(const ::google::protobuf::Message& message, bool single_line_mode,
             ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    LOG(WARNING) << "foo";

    // LOG(WARNING) << "message: " << message.ShortDebugString();
    // LOG(WARNING) << "single_line_mode: " << single_line_mode;
    // ::google::protobuf::TextFormat::MessagePrinter::Print(
    //     message, single_line_mode, generator);
    if (single_line_mode) {
      generator->PrintString(message.ShortDebugString());
      return;
    } else {
      generator->PrintString(message.DebugString());
    }
  }

 private:
  P4InfoManager* p4_info_manager_;
};

class MyFastFieldValuePrinter
    : public ::google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  MyFastFieldValuePrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)) {}

  ~MyFastFieldValuePrinter() override {}

  ::util::Status Initialize() {
    return p4_info_manager_->InitializeAndVerify();
  }

  bool PrintMessageContent(const ::google::protobuf::Message& message,
                           int field_index, int field_count,
                           bool single_line_mode,
                           ::google::protobuf::TextFormat::BaseTextGenerator*
                               generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    return false;
  }

  void PrintMessageEnd(const ::google::protobuf::Message& message,
                       int field_index, int field_count, bool single_line_mode,
                       ::google::protobuf::TextFormat::BaseTextGenerator*
                           generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    if (single_line_mode) {
      generator->PrintLiteral("}hello ");
    } else {
      generator->PrintLiteral("}hello\n");
    }
  }

  void PrintFieldName(const ::google::protobuf::Message& message,
                      const ::google::protobuf::Reflection* reflection,
                      const ::google::protobuf::FieldDescriptor* field,
                      ::google::protobuf::TextFormat::BaseTextGenerator*
                          generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintFieldName(
        message, reflection, field, generator);
  }

  void PrintUInt32(uint32 val,
                   ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintUInt32(
        val, generator);
    ::p4::config::v1::Action a =
        p4_info_manager_->FindActionByID(val).ConsumeValueOrDie();
    generator->PrintString(absl::StrCat("  # ", a.preamble().name()));
  }

 private:
  P4InfoManager* p4_info_manager_;
};

class TableIdPrettyPrinter
    : public ::google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  TableIdPrettyPrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)) {}

  void PrintUInt32(uint32 val,
                   ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintUInt32(
        val, generator);
    auto table = p4_info_manager_->FindTableByID(val);
    if (!table.ok()) {
      return;
    }
    generator->PrintString(
        absl::StrCat("  # ", table.ConsumeValueOrDie().preamble().name()));
  }

 private:
  P4InfoManager* p4_info_manager_;
};

class ActionIdPrettyPrinter
    : public ::google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  ActionIdPrettyPrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)) {}

  void PrintUInt32(uint32 val,
                   ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintUInt32(
        val, generator);
    auto action = p4_info_manager_->FindActionByID(val);
    if (!action.ok()) {
      return;
    }
    generator->PrintString(
        absl::StrCat("  # ", action.ConsumeValueOrDie().preamble().name()));
  }

 private:
  P4InfoManager* p4_info_manager_;
};

class ActionParamPrettyPrinter
    : public ::google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  ActionParamPrettyPrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)), action_id_(0) {}

  void PrintFieldName(const ::google::protobuf::Message& message,
                      const ::google::protobuf::Reflection* reflection,
                      const ::google::protobuf::FieldDescriptor* field,
                      ::google::protobuf::TextFormat::BaseTextGenerator*
                          generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    LOG(WARNING) << message.GetTypeName() << ", " << message.ShortDebugString();
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintFieldName(
        message, reflection, field, generator);

    const ::p4::v1::Action* a = dynamic_cast<const ::p4::v1::Action*>(&message);
    if (!a) {
      return;
    }
    LOG(WARNING) << a->action_id();
    action_id_ = a->action_id();
  }

  bool PrintMessageContent(const ::google::protobuf::Message& message,
                           int field_index, int field_count,
                           bool single_line_mode,
                           ::google::protobuf::TextFormat::BaseTextGenerator*
                               generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    LOG(WARNING) << message.GetTypeName() << ", " << message.ShortDebugString();
    LOG(WARNING) << "field_index: " << field_index;
    LOG(WARNING) << "field_count: " << field_count;
    if (single_line_mode) return false;  // No annotations in single line mode.
    LOG(WARNING) << "action_id_: " << action_id_;
    if (!action_id_) return false;
    auto action = p4_info_manager_->FindActionByID(action_id_);
    if (!action.ok()) {
      return false;
    }
    const ::p4::v1::Action::Param* param =
        dynamic_cast<const ::p4::v1::Action::Param*>(&message);
    if (!param) {
      return false;
    }

    auto p4_info_param =
        p4_info_manager_->FindActionParamByID(action_id_, param->param_id());
    if (!p4_info_param.ok()) {
      return false;
    }

    // Actual Param printing.
    generator->PrintString(absl::StrCat("param_id: ", param->param_id(), "  # ",
                                        p4_info_param.ValueOrDie().name(),
                                        "\n"));
    generator->PrintLiteral("value: ");
    generator->PrintLiteral("\"");
    generator->PrintString(google::protobuf::CEscape(param->value()));
    generator->PrintLiteral("\"");
    generator->PrintLiteral("\n");

    return true;
  }

 private:
  P4InfoManager* p4_info_manager_;
  mutable uint32 action_id_;
};

class FieldMatchPrettyPrinter
    : public ::google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  FieldMatchPrettyPrinter(P4InfoManager* p4_info_manager)
      : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)), table_id_(0) {}

  void PrintFieldName(const ::google::protobuf::Message& message,
                      const ::google::protobuf::Reflection* reflection,
                      const ::google::protobuf::FieldDescriptor* field,
                      ::google::protobuf::TextFormat::BaseTextGenerator*
                          generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    LOG(WARNING) << message.GetTypeName() << ", " << message.ShortDebugString();
    ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintFieldName(
        message, reflection, field, generator);

    const ::p4::v1::TableEntry* t =
        dynamic_cast<const ::p4::v1::TableEntry*>(&message);
    if (!t) {
      return;
    }
    table_id_ = t->table_id();
  }

  bool PrintMessageContent(const ::google::protobuf::Message& message,
                           int field_index, int field_count,
                           bool single_line_mode,
                           ::google::protobuf::TextFormat::BaseTextGenerator*
                               generator) const override {
    LOG(WARNING) << __PRETTY_FUNCTION__;
    LOG(WARNING) << message.GetTypeName() << ", " << message.ShortDebugString();
    LOG(WARNING) << "field_index: " << field_index;
    LOG(WARNING) << "field_count: " << field_count;
    if (single_line_mode) return false;  // No annotations in single line mode.
    if (!table_id_) return false;
    auto table = p4_info_manager_->FindTableByID(table_id_);
    if (!table.ok()) {
      return false;
    }
    const ::p4::v1::FieldMatch* match =
        dynamic_cast<const ::p4::v1::FieldMatch*>(&message);
    if (!match) {
      return false;
    }

    auto p4_info_match =
        p4_info_manager_->FindTableMatchFieldByID(table_id_, match->field_id());
    if (!p4_info_match.ok()) {
      return false;
    }

    // Actual Param printing.
    generator->PrintString(absl::StrCat("field_id: ", match->field_id(), "  # ",
                                        p4_info_match.ValueOrDie().name(),
                                        "\n"));
    // match->field_match_type
    // generator->PrintLiteral("value: ");
    generator->PrintString(match->exact().DebugString());
    // ::google::protobuf::TextFormat::Printer::Print(match, generator);

    // generator->PrintLiteral("\"");
    // generator->PrintString(google::protobuf::CEscape(match->value()));
    // generator->PrintLiteral("\"");
    generator->PrintLiteral("\n");

    return true;
  }

  void PrintUInt32(uint32 val,
                   ::google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    CHECK(false);
    // ::google::protobuf::TextFormat::FastFieldValuePrinter::PrintUInt32(
    //     val, generator);
    // auto action = p4_info_manager_->FindActionByID(val);
    // if (!action.ok()) {
    //   return;
    // }
    // generator->PrintString(
    //     absl::StrCat("  # ", action.ConsumeValueOrDie().preamble().name()));
  }

 private:
  P4InfoManager* p4_info_manager_;
  mutable uint32 table_id_;
};

::util::Status PrettyPrintP4ProtoToString(
    const ::p4::config::v1::P4Info& p4info,
    const ::google::protobuf::Message& message, std::string* text) {
  P4InfoManager mgr(p4info);
  RETURN_IF_ERROR(mgr.InitializeAndVerify());

  ::google::protobuf::TextFormat::Printer p;

  RET_CHECK(p.RegisterFieldValuePrinter(
      ::p4::v1::TableEntry::GetDescriptor()->FindFieldByNumber(
          ::p4::v1::TableEntry::kTableIdFieldNumber),
      new TableIdPrettyPrinter(&mgr)));

  RET_CHECK(p.RegisterFieldValuePrinter(
      ::p4::v1::Action::GetDescriptor()->FindFieldByNumber(
          ::p4::v1::Action::kActionIdFieldNumber),
      new ActionIdPrettyPrinter(&mgr)));

  // Registers a global printer with no way to fall back to the default printer.
  // Also replaces all field printers!
  // RET_CHECK(p.RegisterMessagePrinter(::p4::v1::Action::GetDescriptor(),
  //                                    new ActionPrettyPrinter(&mgr)));

  RET_CHECK(p.RegisterFieldValuePrinter(
      ::p4::v1::Action::GetDescriptor()->FindFieldByNumber(
          ::p4::v1::Action::kParamsFieldNumber),
      new ActionParamPrettyPrinter(&mgr)));

  // This overrides the custom printer below.
  RET_CHECK(p.RegisterFieldValuePrinter(
      ::p4::v1::TableEntry::GetDescriptor()->FindFieldByNumber(
          ::p4::v1::TableEntry::kMatchFieldNumber),
      new FieldMatchPrettyPrinter(&mgr)));

  RET_CHECK(p.RegisterFieldValuePrinter(
      ::p4::v1::FieldMatch::GetDescriptor()->FindFieldByNumber(
          ::p4::v1::FieldMatch::kFieldIdFieldNumber),
      new FieldMatchPrettyPrinter(&mgr)));

  RET_CHECK(p.PrintToString(message, text));

  return ::util::OkStatus();
}

}  // namespace
}  // namespace hal
}  // namespace stratum

#endif
