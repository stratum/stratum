# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: p4/v1/p4data.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='p4/v1/p4data.proto',
  package='p4.v1',
  syntax='proto3',
  serialized_options=None,
  serialized_pb=_b('\n\x12p4/v1/p4data.proto\x12\x05p4.v1\"\x94\x03\n\x06P4Data\x12\x13\n\tbitstring\x18\x01 \x01(\x0cH\x00\x12!\n\x06varbit\x18\x02 \x01(\x0b\x32\x0f.p4.v1.P4VarbitH\x00\x12\x0e\n\x04\x62ool\x18\x03 \x01(\x08H\x00\x12$\n\x05tuple\x18\x04 \x01(\x0b\x32\x13.p4.v1.P4StructLikeH\x00\x12%\n\x06struct\x18\x05 \x01(\x0b\x32\x13.p4.v1.P4StructLikeH\x00\x12!\n\x06header\x18\x06 \x01(\x0b\x32\x0f.p4.v1.P4HeaderH\x00\x12,\n\x0cheader_union\x18\x07 \x01(\x0b\x32\x14.p4.v1.P4HeaderUnionH\x00\x12,\n\x0cheader_stack\x18\x08 \x01(\x0b\x32\x14.p4.v1.P4HeaderStackH\x00\x12\x37\n\x12header_union_stack\x18\t \x01(\x0b\x32\x19.p4.v1.P4HeaderUnionStackH\x00\x12\x0e\n\x04\x65num\x18\n \x01(\tH\x00\x12\x0f\n\x05\x65rror\x18\x0b \x01(\tH\x00\x12\x14\n\nenum_value\x18\x0c \x01(\x0cH\x00\x42\x06\n\x04\x64\x61ta\"/\n\x08P4Varbit\x12\x11\n\tbitstring\x18\x01 \x01(\x0c\x12\x10\n\x08\x62itwidth\x18\x02 \x01(\x05\".\n\x0cP4StructLike\x12\x1e\n\x07members\x18\x01 \x03(\x0b\x32\r.p4.v1.P4Data\"0\n\x08P4Header\x12\x10\n\x08is_valid\x18\x01 \x01(\x08\x12\x12\n\nbitstrings\x18\x02 \x03(\x0c\"Q\n\rP4HeaderUnion\x12\x19\n\x11valid_header_name\x18\x01 \x01(\t\x12%\n\x0cvalid_header\x18\x02 \x01(\x0b\x32\x0f.p4.v1.P4Header\"1\n\rP4HeaderStack\x12 \n\x07\x65ntries\x18\x01 \x03(\x0b\x32\x0f.p4.v1.P4Header\";\n\x12P4HeaderUnionStack\x12%\n\x07\x65ntries\x18\x01 \x03(\x0b\x32\x14.p4.v1.P4HeaderUnionb\x06proto3')
)




_P4DATA = _descriptor.Descriptor(
  name='P4Data',
  full_name='p4.v1.P4Data',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='bitstring', full_name='p4.v1.P4Data.bitstring', index=0,
      number=1, type=12, cpp_type=9, label=1,
      has_default_value=False, default_value=_b(""),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='varbit', full_name='p4.v1.P4Data.varbit', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='bool', full_name='p4.v1.P4Data.bool', index=2,
      number=3, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='tuple', full_name='p4.v1.P4Data.tuple', index=3,
      number=4, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='struct', full_name='p4.v1.P4Data.struct', index=4,
      number=5, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='header', full_name='p4.v1.P4Data.header', index=5,
      number=6, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='header_union', full_name='p4.v1.P4Data.header_union', index=6,
      number=7, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='header_stack', full_name='p4.v1.P4Data.header_stack', index=7,
      number=8, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='header_union_stack', full_name='p4.v1.P4Data.header_union_stack', index=8,
      number=9, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='enum', full_name='p4.v1.P4Data.enum', index=9,
      number=10, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='error', full_name='p4.v1.P4Data.error', index=10,
      number=11, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='enum_value', full_name='p4.v1.P4Data.enum_value', index=11,
      number=12, type=12, cpp_type=9, label=1,
      has_default_value=False, default_value=_b(""),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
    _descriptor.OneofDescriptor(
      name='data', full_name='p4.v1.P4Data.data',
      index=0, containing_type=None, fields=[]),
  ],
  serialized_start=30,
  serialized_end=434,
)


_P4VARBIT = _descriptor.Descriptor(
  name='P4Varbit',
  full_name='p4.v1.P4Varbit',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='bitstring', full_name='p4.v1.P4Varbit.bitstring', index=0,
      number=1, type=12, cpp_type=9, label=1,
      has_default_value=False, default_value=_b(""),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='bitwidth', full_name='p4.v1.P4Varbit.bitwidth', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=436,
  serialized_end=483,
)


_P4STRUCTLIKE = _descriptor.Descriptor(
  name='P4StructLike',
  full_name='p4.v1.P4StructLike',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='members', full_name='p4.v1.P4StructLike.members', index=0,
      number=1, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=485,
  serialized_end=531,
)


_P4HEADER = _descriptor.Descriptor(
  name='P4Header',
  full_name='p4.v1.P4Header',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='is_valid', full_name='p4.v1.P4Header.is_valid', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='bitstrings', full_name='p4.v1.P4Header.bitstrings', index=1,
      number=2, type=12, cpp_type=9, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=533,
  serialized_end=581,
)


_P4HEADERUNION = _descriptor.Descriptor(
  name='P4HeaderUnion',
  full_name='p4.v1.P4HeaderUnion',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='valid_header_name', full_name='p4.v1.P4HeaderUnion.valid_header_name', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='valid_header', full_name='p4.v1.P4HeaderUnion.valid_header', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=583,
  serialized_end=664,
)


_P4HEADERSTACK = _descriptor.Descriptor(
  name='P4HeaderStack',
  full_name='p4.v1.P4HeaderStack',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='entries', full_name='p4.v1.P4HeaderStack.entries', index=0,
      number=1, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=666,
  serialized_end=715,
)


_P4HEADERUNIONSTACK = _descriptor.Descriptor(
  name='P4HeaderUnionStack',
  full_name='p4.v1.P4HeaderUnionStack',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='entries', full_name='p4.v1.P4HeaderUnionStack.entries', index=0,
      number=1, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=717,
  serialized_end=776,
)

_P4DATA.fields_by_name['varbit'].message_type = _P4VARBIT
_P4DATA.fields_by_name['tuple'].message_type = _P4STRUCTLIKE
_P4DATA.fields_by_name['struct'].message_type = _P4STRUCTLIKE
_P4DATA.fields_by_name['header'].message_type = _P4HEADER
_P4DATA.fields_by_name['header_union'].message_type = _P4HEADERUNION
_P4DATA.fields_by_name['header_stack'].message_type = _P4HEADERSTACK
_P4DATA.fields_by_name['header_union_stack'].message_type = _P4HEADERUNIONSTACK
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['bitstring'])
_P4DATA.fields_by_name['bitstring'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['varbit'])
_P4DATA.fields_by_name['varbit'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['bool'])
_P4DATA.fields_by_name['bool'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['tuple'])
_P4DATA.fields_by_name['tuple'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['struct'])
_P4DATA.fields_by_name['struct'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['header'])
_P4DATA.fields_by_name['header'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['header_union'])
_P4DATA.fields_by_name['header_union'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['header_stack'])
_P4DATA.fields_by_name['header_stack'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['header_union_stack'])
_P4DATA.fields_by_name['header_union_stack'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['enum'])
_P4DATA.fields_by_name['enum'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['error'])
_P4DATA.fields_by_name['error'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4DATA.oneofs_by_name['data'].fields.append(
  _P4DATA.fields_by_name['enum_value'])
_P4DATA.fields_by_name['enum_value'].containing_oneof = _P4DATA.oneofs_by_name['data']
_P4STRUCTLIKE.fields_by_name['members'].message_type = _P4DATA
_P4HEADERUNION.fields_by_name['valid_header'].message_type = _P4HEADER
_P4HEADERSTACK.fields_by_name['entries'].message_type = _P4HEADER
_P4HEADERUNIONSTACK.fields_by_name['entries'].message_type = _P4HEADERUNION
DESCRIPTOR.message_types_by_name['P4Data'] = _P4DATA
DESCRIPTOR.message_types_by_name['P4Varbit'] = _P4VARBIT
DESCRIPTOR.message_types_by_name['P4StructLike'] = _P4STRUCTLIKE
DESCRIPTOR.message_types_by_name['P4Header'] = _P4HEADER
DESCRIPTOR.message_types_by_name['P4HeaderUnion'] = _P4HEADERUNION
DESCRIPTOR.message_types_by_name['P4HeaderStack'] = _P4HEADERSTACK
DESCRIPTOR.message_types_by_name['P4HeaderUnionStack'] = _P4HEADERUNIONSTACK
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

P4Data = _reflection.GeneratedProtocolMessageType('P4Data', (_message.Message,), dict(
  DESCRIPTOR = _P4DATA,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4Data)
  ))
_sym_db.RegisterMessage(P4Data)

P4Varbit = _reflection.GeneratedProtocolMessageType('P4Varbit', (_message.Message,), dict(
  DESCRIPTOR = _P4VARBIT,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4Varbit)
  ))
_sym_db.RegisterMessage(P4Varbit)

P4StructLike = _reflection.GeneratedProtocolMessageType('P4StructLike', (_message.Message,), dict(
  DESCRIPTOR = _P4STRUCTLIKE,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4StructLike)
  ))
_sym_db.RegisterMessage(P4StructLike)

P4Header = _reflection.GeneratedProtocolMessageType('P4Header', (_message.Message,), dict(
  DESCRIPTOR = _P4HEADER,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4Header)
  ))
_sym_db.RegisterMessage(P4Header)

P4HeaderUnion = _reflection.GeneratedProtocolMessageType('P4HeaderUnion', (_message.Message,), dict(
  DESCRIPTOR = _P4HEADERUNION,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4HeaderUnion)
  ))
_sym_db.RegisterMessage(P4HeaderUnion)

P4HeaderStack = _reflection.GeneratedProtocolMessageType('P4HeaderStack', (_message.Message,), dict(
  DESCRIPTOR = _P4HEADERSTACK,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4HeaderStack)
  ))
_sym_db.RegisterMessage(P4HeaderStack)

P4HeaderUnionStack = _reflection.GeneratedProtocolMessageType('P4HeaderUnionStack', (_message.Message,), dict(
  DESCRIPTOR = _P4HEADERUNIONSTACK,
  __module__ = 'p4.v1.p4data_pb2'
  # @@protoc_insertion_point(class_scope:p4.v1.P4HeaderUnionStack)
  ))
_sym_db.RegisterMessage(P4HeaderUnionStack)


# @@protoc_insertion_point(module_scope)
