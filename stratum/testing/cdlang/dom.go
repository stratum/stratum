// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Package cdl provides internals of the CDLang transpiler.
package cdl

// This file implements document object model - an object that is used to store
// information extracted from the input files written in CDLang and which is
// passed as an input to the Go template engine.

import (
	"encoding/json"
	"fmt"
	"math"
	"math/rand"
	"regexp"
	"strconv"
	"strings"

	"log"
)

type withParams interface {
	addParam(*Param)
	Param() *Param
}

type instructionGroup interface {
	addInstruction(instruction *Instruction)
	// Vars returns all variables that are used in this instruction group.
	Vars() []*Param
}

// InstType defines type of CDLang instruction.
type InstType int

const (
	// SendInst indicates a send instruction.
	SendInst InstType = iota
	// ReceiveInst indicates a receive instruction.
	ReceiveInst
	// ExecuteInst indicates an execute instruction.
	ExecuteInst
	// GroupAnyOrderInst indicates an any order grouping instruction.
	GroupAnyOrderInst
	// GroupZeroOrMoreInst indicates a zero-or-more grouping instruction.
	GroupZeroOrMoreInst
	// GroupAtLeastOnceInst indicates an at-least-once grouping instruction.
	GroupAtLeastOnceInst
	// VarDeclarationInst indicates a instruction declaring a variable.
	VarDeclarationInst
	// ScenarioInst indicates a sceanario instruction.
	ScenarioInst
	// SubScenarioInst indicates a sub-sceanrtio instruction.
	SubScenarioInst
	// OpenGNMIStrInst indicates an open-GNMI-stream instruction.
	OpenGNMIStrInst
	// OpenCTRLStrInst indicates an open-CTRL-stream instruction.
	OpenCTRLStrInst
	// CloseStrInst indicates a close-stream instruction.
	CloseStrInst
	// CallInst indicates a gRPC call instruction.
	CallInst
	// ConstProtoInst indicates a PROTOBUF that will be reused in scenarios.
	ConstProtoInst
)

var instTypeToString = map[InstType]string{
	SendInst:             "send",
	ReceiveInst:          "receive",
	ExecuteInst:          "execute",
	GroupZeroOrMoreInst:  "zero_or_more",
	GroupAtLeastOnceInst: "at_least_once",
	GroupAnyOrderInst:    "any_order",
	VarDeclarationInst:   "var_declaration",
	ScenarioInst:         "scenario",
	SubScenarioInst:      "sub_scenario",
	OpenGNMIStrInst:      "open_gnmi_stream",
	OpenCTRLStrInst:      "open_ctrl_stream",
	CloseStrInst:         "close_stream",
	CallInst:             "call",
	ConstProtoInst:       "const-proto",
}

// String returns a string specifying type of instruction. Used for debugging.
func (s InstType) String() string {
	if name, ok := instTypeToString[s]; ok {
		return name
	}
	return "?"
}

// Instruction is an CDLang instruction like send, receive, etc.
type Instruction struct {
	// Valid for all types of instructions.
	Type InstType
	Name string
	ID   int32
	// Valid for Call, Send and Receive only.
	Protobuf *protobuf `json:",omitempty"` // Protobuf to be sent or received.
	// Valid for Call only.
	Response      *protobuf `json:",omitempty"` // Protobuf expected as a call response.
	ErrorExpected bool      `json:",omitempty"` // Expected error status of the call.
	// Valid for Send and Receive only.
	Channel string `json:",omitempty"` // The name of the stream.
	// Valid for receive only.
	Next *Instruction `json:",omitempty"` // Another message that can be recived at this point.
	Skip bool         `json:",omitempty"` // This instruction is received in preciding loop.
	// Valid for varDeclaration only.
	Variable *Param `json:",omitempty"`
	// Valid for execute only.
	Params []*Param `json:",omitempty"`
	// Instructions that are frouped by this instruction.
	Children []*Instruction `json:",omitempty"`
	// Valid for sceanrio and sub-scenario only
	Version  *Version `json:",omitempty"`
	Disabled bool     `json:",omitempty"` // Set to true if scenario should not be executed.
	Source   string   `json:",omitempty"` // The source code of the (sub-)scenario.
	// Valid for AnyOrder group only.
	ChildrenPerChannel map[string][]*Instruction `json:",omitempty"`
}

// shortDebugString returns a debug string showing details of the instruction.
func (s *Instruction) shortDebugString(prefix ...string) string {
	return fmt.Sprintf("%s%s type: %s", strings.Join(prefix, ""), s.Name, s.Type.String())
}

// IsInstructionGroup returns true is the isnstruction is not a simple instruction.
func (s *Instruction) IsInstructionGroup() bool {
	return len(s.Children) > 0
}

// IsInstType returns true if the instruction is of specified type.
func (s *Instruction) IsInstType(instructionType InstType) bool {
	return instructionType == s.Type
}

func (s *Instruction) addParam(parameter *Param) {
	s.Params = append(s.Params, parameter)
}

// Param returns the first parameter of this instruction.
func (s *Instruction) Param() *Param {
	if len(s.Params) > 0 {
		return s.Params[0]
	}
	return nil
}

func (s *Instruction) addInstruction(instruction *Instruction) {
	s.Children = append(s.Children, instruction)
}

// Vars returns a list of all variables that are used by children of this instruction.
func (s *Instruction) Vars() []*Param {
	// Find all variables that are used by children of this instruction.
	unique := map[string]*Param{}
	for _, i := range s.Children {
		if i.Protobuf != nil {
			for _, v := range i.Protobuf.Vars() {
				unique[v.Name] = v
			}
		} else if i.IsInstructionGroup() {
			for _, v := range i.Vars() {
				unique[v.Name] = v
			}
		}
	}
	input := map[string]*Param{}
	// If this is a sub-subscenario instruction do not return input parameters as variables.
	if s.Type == SubScenarioInst {
		for _, i := range s.Params {
			input[i.Name] = i
		}
	}
	result := []*Param{}
	for _, v := range unique {
		if _, ok := input[v.Name]; !ok {
			result = append(result, v)
		}
	}
	return result
}

func (s *Instruction) setVersion(major, minor, patch int64) {
	s.Version.Major = major
	s.Version.Minor = minor
	s.Version.Patch = patch
}

var protoNameToFuncName = map[string]string{
	// gNMI Requests
	"CapabilityRequest": "Capabilities",
	"GetRequest":        "Get",
	"SetRequest":        "Set",
	"SubscribeRequest":  "Subscribe",
	// gNMI Responses
	"CapabilityResponse": "Capabilities",
	"GetResponse":        "Get",
	"SetResponse":        "Set",
	"SubscribeResponse":  "Subscribe",
	// ctrl Request
	"ExecuteRequest": "Execute",
	// ctrl Response
	"ExecuteResponse": "Execute",
}

var protoNameToNamespace = map[string]string{
	// gNMI Requests
	"CapabilityRequest": "gnmi",
	"GetRequest":        "gnmi",
	"SetRequest":        "gnmi",
	"SubscribeRequest":  "gnmi",
	// gNMI Responses
	"CapabilityResponse": "gnmi",
	"GetResponse":        "gnmi",
	"SetResponse":        "gnmi",
	"SubscribeResponse":  "gnmi",
	// ctrl Request
	"ExecuteRequest": "ctrl",
	// ctrl Response
	"ExecuteResponse": "ctrl",
}

// FuncName returns gRPC function name that accepts the type of protobuf specified by Protobuf field.
func (s *Instruction) FuncName() string {
	if name, ok := protoNameToFuncName[s.Protobuf.TypeName]; ok {
		return name
	}
	return "?"
}

// A CDLang scenario and sub-scenario are versioned.
type versioned interface {
	setVersion(major, minor, patch int64)
}

// Version stores the version of a scenario in major.minor.patch form.
type Version struct {
	Major int64
	Minor int64
	Patch int64
}

var re = regexp.MustCompile("(?P<major>([0-9]+)).(?P<minor>([0-9]+)).(?P<patch>([0-9]+))")

// NewVersion converts version in a form of a string into Version object.
func NewVersion(ver string) (*Version, error) {
	if ver == "latest" {
		return &Version{Major: math.MaxInt64, Minor: math.MaxInt64, Patch: math.MaxInt64}, nil
	}
	match := re.FindStringSubmatch(ver)
	m := make(map[string]string)
	for i, name := range re.SubexpNames() {
		if i != 0 && len(match) > i && len(name) > 0 {
			m[name] = match[i]
		}
	}
	major, err := strconv.ParseInt(m["major"], 10, 64)
	if err != nil {
		return nil, fmt.Errorf("while parsing major part of version string '%s': %v", ver, err)
	}
	minor, err := strconv.ParseInt(m["minor"], 10, 64)
	if err != nil {
		return nil, fmt.Errorf("while parsing minor part of version string '%s': %v", ver, err)
	}
	patch, err := strconv.ParseInt(m["patch"], 10, 64)
	if err != nil {
		return nil, fmt.Errorf("while parsing patch part of version string '%s': %v", ver, err)
	}
	return &Version{Major: major, Minor: minor, Patch: patch}, nil
}

// Compare returns 0 if both versions are equal, a negative number if v is lower version, a positive number otherwise.
func (v Version) Compare(o *Version) int64 {
	if o == nil {
		return 1
	}
	major := v.Major - o.Major
	if major != 0 {
		return major
	}
	minor := v.Minor - o.Minor
	if minor != 0 {
		return minor
	}
	return v.Patch - o.Patch
}

func (v Version) String() string {
	if v.Major == math.MaxInt64 && v.Minor == math.MaxInt64 && v.Patch == math.MaxInt64 {
		return "latest"
	}
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

// Returns initialized instance of scenario.
func newScenario(name string) *Instruction {
	return &Instruction{
		Type:    ScenarioInst,
		Name:    name,
		Version: &Version{},
	}
}

// Returns initialized instance of subScenario.
func newSubScenario(name string) *Instruction {
	return &Instruction{
		Type:    SubScenarioInst,
		Name:    name,
		Version: &Version{},
	}
}

// newOpenGNMIStr returns a CDLang instruction that opens a streaming gNMI interface.
func newOpenGNMIStr(channel string) *Instruction {
	return &Instruction{
		Type:    OpenGNMIStrInst,
		Name:    "OpenGNMIStream",
		Channel: channel,
	}
}

// newOpenCTRLStr returns a CDLang instruction that opens a streaming ctrl interface.
func newOpenCTRLStr(channel string) *Instruction {
	return &Instruction{
		Type:    OpenCTRLStrInst,
		Name:    "OpenCTRLStream",
		Channel: channel,
	}
}

// newCloseStr returns a CDLang instruction that closes an open streaming gRPC interface.
func newCloseStr(name, channel string) *Instruction {
	return &Instruction{
		Type:    CloseStrInst,
		Name:    name,
		Channel: channel,
	}
}

// newCall returns a CDLang instruction that performs gNMI GET or SET operation.
func newCall(name string) *Instruction {
	return &Instruction{
		Type: CallInst,
		Name: name,
	}
}

// newSend returns a CDLang instruction that transmits a Protobuf.
func newSend(name string, channel string) *Instruction {
	return &Instruction{
		Type:    SendInst,
		Name:    name,
		Channel: channel,
	}
}

// newReceive returns a CDLang instruction that receives a Protobuf.
func newReceive(name string, channel string) *Instruction {
	return &Instruction{
		Type:    ReceiveInst,
		Name:    name,
		Channel: channel,
	}
}

// Returns initialized instance of execute.
func newExecute(name string) *Instruction {
	return &Instruction{
		Type: ExecuteInst,
		Name: name,
	}
}

// Returns initialized instance of varDeclaration.
func newVarDeclaration(variable *Param) *Instruction {
	return &Instruction{
		Type:     VarDeclarationInst,
		Name:     variable.Name,
		Variable: variable,
	}
}

// Returns initialized instance of groupAnyOrder.
func newGroupAnyOrder() *Instruction {
	return &Instruction{
		Type: GroupAnyOrderInst,
		Name: "AnyOrder",
	}
}

// Returns initialized instance of groupAtLeastOnce.
func newGroupAtLeastOnce() *Instruction {
	return &Instruction{
		Type: GroupAtLeastOnceInst,
		Name: "AtLeastOnce",
	}
}

// Returns initialized instance of groupZeroOrMore.
func newGroupZeroOrMore() *Instruction {
	return &Instruction{
		Type: GroupZeroOrMoreInst,
		Name: "ZeroOrMore",
	}
}

// Returns initialized instance of constProto.
func newConstProto() *Instruction {
	return &Instruction{
		Type: ConstProtoInst,
		Name: "ConstProto",
	}
}

// ParameterKind defines the type of the parameter.
// ParameterKind defines the type of the parameter.
// A CDLang instruction paramater can be either a variable or a constant.
type ParameterKind int

const (
	variable ParameterKind = iota
	stringConstant
	otherConstant
	declaration
	ignored
	variableDecl
)

var paramTypeToString = map[ParameterKind]string{
	variable:       "variable",
	declaration:    "declaration",
	stringConstant: "string",
	otherConstant:  "other",
	ignored:        "ignored",
	variableDecl:   "variableDecl",
}

// IsType returns true if the input parameter is equal to the name of the kind of the parameter.
func (p *ParameterKind) IsType(paramType string) bool {
	if name, ok := paramTypeToString[*p]; ok {
		return name == paramType
	}
	return false
}

// Param is a CDLang instruction run-time paramater.
// Some instructions as `execute` accept parameters.
type Param struct {
	ParameterKind ParameterKind
	Name          string
	ParameterType string
}

// Ignore returns 'true' if the parameter is marked as 'to be ignored'.
func (p *Param) Ignore() bool {
	return p.ParameterKind == ignored
}

// String returns the parameter in a form that can be used in the template.
func (p *Param) String() string {
	switch p.ParameterKind {
	case variable, variableDecl:
		return strconv.Quote("$" + p.Name)
	case declaration:
		return p.Name
	case stringConstant:
		return strconv.Quote(p.Name)
	case otherConstant:
		return p.Name
	case ignored:
		return ""
	default:
		return "<not supported>"
	}
}

// Returns initialized instance of param.
func newVariable(name, paramType string) *Param {
	return &Param{
		ParameterKind: variable,
		Name:          name,
		ParameterType: paramType,
	}
}

// Returns initialized instance of param.
func newDeclaration(name, paramType string) *Param {
	return &Param{
		ParameterKind: declaration,
		Name:          name,
		ParameterType: paramType,
	}
}

// Returns initialized instance of Param.
func newStringConstant(name string) *Param {
	return &Param{
		ParameterKind: stringConstant,
		Name:          name,
		ParameterType: "string",
	}
}

// Returns initialized instance of Param.
func newConstant(name, paramType string) *Param {
	return &Param{
		ParameterKind: otherConstant,
		Name:          name,
		ParameterType: paramType,
	}
}

// Returns initialized instance of Param.
func newIgnoredValue() *Param {
	return &Param{
		ParameterKind: ignored,
		Name:          "<ignored>",
		ParameterType: "<ignored>",
	}
}

type gnmiPathElem struct {
	Name       string
	Key        map[string]*Param `json:",omitempty"`
	Parameters []*Param          `json:",omitempty"`
}

// Returns initialized instance of gnmiPathElem.
func newGNMIPathElem() *gnmiPathElem {
	return &gnmiPathElem{
		Key: map[string]*Param{},
	}
}

func (p *gnmiPathElem) String() string {
	return strconv.Quote(p.Name)
}

func (p *gnmiPathElem) Ignore() bool {
	param := p.Param()
	if param == nil {
		return false
	}
	return param.Ignore()
}

// IsType returns true if the input parameter is equal to the name of the type of the field.
func (p *gnmiPathElem) IsType(fieldType string) bool {
	param := p.Param()
	if param == nil {
		return false
	}
	return param.ParameterKind.IsType(fieldType)
}

func (p *gnmiPathElem) addParam(param *Param) {
	p.Parameters = append(p.Parameters, param)
}

func (p *gnmiPathElem) Params() []*Param {
	result := p.Parameters
	for _, v := range p.Key {
		switch v.ParameterKind {
		case variable, variableDecl:
			result = append(result, v)
		}
	}
	return result
}

func (p *gnmiPathElem) Param() *Param {
	if v := p.Params(); len(v) > 0 {
		return v[0]
	}
	return nil
}

type gnmiPath struct {
	Elem []*gnmiPathElem `json:",omitempty"`
}

func newGNMIPath() *gnmiPath {
	return &gnmiPath{}
}

func (p *gnmiPath) addExistingElem(elem *gnmiPathElem) {
	p.Elem = append(p.Elem, elem)
}

func (p *gnmiPath) addElem(name string) {
	p.Elem = append(p.Elem, &gnmiPathElem{Name: name, Key: map[string]*Param{}})
}

func (p *gnmiPath) addElemKeyValue(name, key string, parameter *Param) {
	p.Elem = append(p.Elem, &gnmiPathElem{Name: name, Key: map[string]*Param{key: parameter}})
}

type protobufFieldType int

const (
	stringField protobufFieldType = iota
	numberField
	gnmiPathField
	enumField
	variableField
	sequenceOfFields
	repeatedField
	serializedSequenceOfFields
)

var fieldTypeToString = map[protobufFieldType]string{
	stringField:                "string",
	numberField:                "number",
	gnmiPathField:              "gnmiPath",
	enumField:                  "enum",
	variableField:              "variable",
	sequenceOfFields:           "sequence",
	repeatedField:              "repeated",
	serializedSequenceOfFields: "serialized",
}

type textproto struct {
	Meta *struct {
		Name string
		Type string
	}
	Text string
}

type protobufField interface {
	setName(name string)
	setValue(val interface{}) bool
	IsSimple() bool
	IsRepeated() bool
	IsType(fieldType string) bool
	Ignore() bool
	GetTextproto() []textproto
}

type genericProtobufField struct {
	Name      string
	FieldType protobufFieldType
}

// Returns initialized instance of genericProtobufField.
func newGenericProtobufField(fieldType protobufFieldType) *genericProtobufField {
	return &genericProtobufField{FieldType: fieldType}
}

func (f *genericProtobufField) setName(name string) {
	f.Name = name
}

func (f *genericProtobufField) IsSimple() bool {
	return true
}

func (f *genericProtobufField) IsRepeated() bool {
	return false
}

func (f *genericProtobufField) IsType(fieldType string) bool {
	if name, ok := fieldTypeToString[f.FieldType]; ok {
		return name == fieldType
	}
	return false
}

func (f *genericProtobufField) Ignore() bool {
	return false
}

func (f *genericProtobufField) GetTextproto() []textproto {
	return []textproto{{nil, fmt.Sprintf("%s: <something>", f.Name)}}
}

type protobufFieldString struct {
	*genericProtobufField
	Value string
}

// Returns initialized instance of protobufFieldString.
func newProtobufFieldString() *protobufFieldString {
	return &protobufFieldString{
		genericProtobufField: newGenericProtobufField(stringField),
	}
}

func (f *protobufFieldString) setValue(val interface{}) bool {
	if v, ok := val.(string); ok {
		f.Value = v
		return true
	}
	return false
}

func (f *protobufFieldString) String() string {
	return strconv.Quote(f.Value)
}

func (f *protobufFieldString) GetTextproto() []textproto {
	return []textproto{{nil, fmt.Sprintf("%s: \"%s\" ", f.Name, f.Value)}}
}

type protobufFieldVariable struct {
	*genericProtobufField
	Parameters []*Param `json:",omitempty"`
}

// Returns initialized instance of protobufFieldVariable.
func newProtobufFieldVariable() *protobufFieldVariable {
	return &protobufFieldVariable{
		genericProtobufField: newGenericProtobufField(variableField),
	}
}

func (f *protobufFieldVariable) setValue(val interface{}) bool {
	if p, ok := val.(*Param); ok {
		f.addParam(p)
		return true
	}
	return false
}

func (f *protobufFieldVariable) String() string {
	return f.Param().String()
}

func (f *protobufFieldVariable) Ignore() bool {
	return f.Param().ParameterKind == ignored
}

func (f *protobufFieldVariable) addParam(parameter *Param) {
	f.Parameters = append(f.Parameters, parameter)
}

func (f *protobufFieldVariable) Params() []*Param {
	return f.Parameters
}

func (f *protobufFieldVariable) Param() *Param {
	if len(f.Parameters) > 0 {
		return f.Parameters[0]
	}
	return nil
}

func (f *protobufFieldVariable) GetTextproto() []textproto {
	return []textproto{{nil, fmt.Sprintf("%s: %s ", f.Name, f.Param().String())}}
}

type protobufFieldInt struct {
	*genericProtobufField
	Value int64
}

// Returns initialized instance of protobufFieldInt.
func newProtobufFieldInt() *protobufFieldInt {
	return &protobufFieldInt{
		genericProtobufField: newGenericProtobufField(numberField),
	}
}

func (f *protobufFieldInt) setValue(val interface{}) bool {
	if s, ok := val.(string); ok {
		if v, err := strconv.ParseInt(s, 10, 64); err == nil {
			f.Value = v
			return true
		}
	}
	return false
}

func (f *protobufFieldInt) String() string {
	return strconv.FormatInt(f.Value, 10)
}

func (f *protobufFieldInt) GetTextproto() []textproto {
	return []textproto{{nil, fmt.Sprintf("%s: %v ", f.Name, f.Value)}}
}

type protobufFieldEnum struct {
	*genericProtobufField
	Value string
}

// Returns initialized instance of protobufFieldEnum.
func newProtobufFieldEnum() *protobufFieldEnum {
	return &protobufFieldEnum{
		genericProtobufField: newGenericProtobufField(enumField),
	}
}

func (f *protobufFieldEnum) setValue(val interface{}) bool {
	if v, ok := val.(string); ok {
		f.Value = v
		return true
	}
	return false
}

func (f *protobufFieldEnum) String() string {
	return f.Value
}

func (f *protobufFieldEnum) GetTextproto() []textproto {
	return []textproto{{nil, fmt.Sprintf("%s: %s ", f.Name, f.Value)}}
}

type protobufFieldGNMIPath struct {
	*genericProtobufField
	Value *gnmiPath
}

// Returns initialized instance of protobufFieldGNMIPath.
func newProtobufFieldGnmiPath() *protobufFieldGNMIPath {
	return &protobufFieldGNMIPath{
		genericProtobufField: newGenericProtobufField(gnmiPathField),
	}
}

func (f *protobufFieldGNMIPath) setValue(val interface{}) bool {
	if v, ok := val.(*gnmiPath); ok {
		f.Value = v
		return true
	}
	return false
}

// String returns a string specifying the gNMI YANG model path in compact, human-readable form.
func (f *protobufFieldGNMIPath) String() string {
	var sb strings.Builder
	for _, elem := range f.Value.Elem {
		fmt.Fprintf(&sb, "/%s", elem.Name)
		if len(elem.Key) > 0 {
			sb.WriteString("[")
			for key, val := range elem.Key {
				fmt.Fprintf(&sb, "%s=%s", key, val)
			}
			sb.WriteString("]")
		}
	}
	return sb.String()
}

func (f *protobufFieldGNMIPath) IsSimple() bool {
	return false
}

func (f *protobufFieldGNMIPath) GetTextproto() []textproto {
	var sb strings.Builder
	sb.WriteString("\npath { ")
	for _, elem := range f.Value.Elem {
		fmt.Fprintf(&sb, "\nelem { name: \"%s\"", elem.Name)
		if len(elem.Key) > 0 {
			for key, val := range elem.Key {
				fmt.Fprintf(&sb, " key { key: \"%s\" value: %s }", key, val)
			}
		}
		sb.WriteString(" }")
	}
	sb.WriteString("\n}")
	return []textproto{{nil, sb.String()}}
}

type protobufFieldGroup interface {
	addField(field protobufField)
	LastField() protobufField
	Vars() []*Param
}

type protobufFieldSequenceSize int

const (
	protobufFieldSequenceSizeOne protobufFieldSequenceSize = iota
	protobufFieldSequenceSizeZeroOrOne
	protobufFieldSequenceSizeZeroOrMore
	protobufFieldSequenceSizeOneOrMore
)

var sequenceSizeToString = map[protobufFieldSequenceSize]string{
	protobufFieldSequenceSizeOne:        "1",
	protobufFieldSequenceSizeZeroOrOne:  "?",
	protobufFieldSequenceSizeZeroOrMore: "*",
	protobufFieldSequenceSizeOneOrMore:  "+",
}

// Used to represent a complex (as in: not-simple) protobuf field.
type protobufFieldSequence struct {
	*genericProtobufField
	Fields       []protobufField `json:",omitempty"`
	SequenceSize protobufFieldSequenceSize
	CastType     string
}

// Returns initialized instance of protobufFieldSequence.
func newProtobufFieldSequence(serialized bool) *protobufFieldSequence {
	if serialized {
		return &protobufFieldSequence{
			genericProtobufField: newGenericProtobufField(serializedSequenceOfFields),
		}
	}
	return &protobufFieldSequence{
		genericProtobufField: newGenericProtobufField(sequenceOfFields),
	}
}

// Returns initialized instance of protobufFieldRepeated.
func newProtobufFieldRepeated() *protobufFieldSequence {
	return &protobufFieldSequence{
		genericProtobufField: newGenericProtobufField(repeatedField),
	}
}

// Returns initialized instance of protobufFieldRepeatedRow.
func newProtobufFieldRepeatedRow() *protobufFieldSequence {
	return &protobufFieldSequence{
		genericProtobufField: newGenericProtobufField(sequenceOfFields),
	}
}

func (p *protobufFieldSequence) addField(field protobufField) {
	p.Fields = append(p.Fields, field)
}

func (p *protobufFieldSequence) LastField() protobufField {
	if len(p.Fields) == 0 {
		return nil
	}
	return p.Fields[len(p.Fields)-1]
}

// IsSize returns true is its parameter is equal to this sequence multiplication spec.
func (p *protobufFieldSequence) IsSize(s string) bool {
	if ps, ok := sequenceSizeToString[p.SequenceSize]; ok {
		return ps == s
	}
	return false
}

func (p *protobufFieldSequence) IsSerialized() bool {
	return p.FieldType == serializedSequenceOfFields
}

func (p *protobufFieldSequence) IsSimple() bool {
	return false
}

func (p *protobufFieldSequence) IsRepeated() bool {
	return p.FieldType == repeatedField
}

func (p *protobufFieldSequence) Vars() []*Param {
	unique := map[string]*Param{}
	for _, f := range p.Fields {
		switch v := f.(type) {
		case *protobufFieldVariable:
			unique[v.Param().Name] = v.Param()
		case *protobufFieldGNMIPath:
			if v := v.Value.Elem; v != nil {
				for _, e := range v {
					if v := e.Param(); v != nil {
						unique[v.Name] = v
					}
				}
			}
		case *protobufFieldSequence:
			for _, v := range v.Vars() {
				unique[v.Name] = v
			}
		}
	}
	result := []*Param{}
	for _, v := range unique {
		result = append(result, v)
	}
	return result
}

func (p *protobufFieldSequence) setValue(val interface{}) bool {
	if v, ok := val.([]protobufField); ok {
		p.Fields = v
		return true
	}
	return false
}

// RandomString - Generate a random string of A-Z chars with len
func RandomString(len int) string {
	bytes := make([]byte, len)
	for i := 0; i < len; i++ {
		bytes[i] = byte(65 + rand.Intn(25)) // A=65 and Z = 65+25
	}
	return string(bytes)
}

func (p *protobufFieldSequence) GetTextproto() []textproto {
	ret := []textproto{}
	result := []string{}
	var m *struct{ Name, Type string }
	if p.IsSerialized() {
		n := "$$" + RandomString(16)
		ret = append(ret, textproto{nil, p.Name + ": \"" + n + "\""})
		m = &struct{ Name, Type string }{n, p.CastType}
	}
	if len(p.Name) > 0 && p.FieldType != repeatedField && !p.IsSerialized() {
		result = append(result, p.Name)
		result = append(result, "{")
	}
	for _, f := range p.Fields {
		for _, t := range f.GetTextproto() {
			if t.Meta != nil {
				ret = append(ret, t)
			} else {
				result = append(result, t.Text)
			}
		}
	}
	if len(p.Name) > 0 && p.FieldType != repeatedField && !p.IsSerialized() {
		result = append(result, "}")
	}
	return append(ret, textproto{m, strings.Join(result[:], " ")})
}

// Used to represent a ProtoBuf as a whole entity.
type protobuf struct {
	*protobufFieldSequence
	TypeName string
}

// Returns initialized instance of Protobuf.
func newProtobuf(typeName string) *protobuf {
	return &protobuf{
		protobufFieldSequence: newProtobufFieldSequence(false),
		TypeName:              typeName,
	}
}

// Namespace returns namespace of protobuf type specified by Protobuf field.
func (p protobuf) Namespace() string {
	if n, ok := protoNameToNamespace[p.TypeName]; ok {
		return n
	}
	return "?"
}

// Path describes an YANG schema path. This type is used to keep information
// about all paths that are covered by scenarios.
type Path struct {
	Path      string                          `json:",omitempty"` // The path as a text string.
	Protobuf  *protobufFieldGNMIPath          `json:",omitempty"` // The path in protobuf format.
	Scenarios map[GNMIReqMode]map[string]bool `json:",omitempty"` // List of scenarios that cover this path.
}

// GNMIReqMode specifies the type of a gNMI request.
type GNMIReqMode int

const (
	// Get denotes a gNMI GET type of request.
	Get GNMIReqMode = iota
	// Set denotes a gNMI SET type of request.
	Set
	// SubscribePoll denotes a gNMI SUBSCRIBE POLL type of request.
	SubscribePoll
	// SubscribeOnce denotes a gNMI SUBSCRIBE ONCE type of request.
	SubscribeOnce
	// SubscribeOnChange denotes a gNMI SUBSCRIBE STREAM:ON_CHANGE type of request.
	SubscribeOnChange
	// SubscribeSample denotes a gNMI SUBSCRIBE STREAM:SAMPLE type of request.
	SubscribeSample
	// SubscribeTargetDefined denotes a gNMI SUBSCRIBE STREAM:TARGET_DEFINED type of request.
	SubscribeTargetDefined
	// Response denotes a gNMI response to a gNMI request.
	Response
	// Other denotes a not interesting type of request. Most likely non-gNMI one.
	Other
)

var gnmiReqModeToString = map[GNMIReqMode]string{
	Get:                    "GET",
	Set:                    "SET",
	SubscribePoll:          "SUBSCRIBE:POLL",
	SubscribeOnce:          "SUBSCRIBE:ONCE",
	SubscribeOnChange:      "SUBSCRIBE:STREAM:ON_CHANGE",
	SubscribeSample:        "SUBSCRIBE:STREAM:SAMPLE",
	SubscribeTargetDefined: "SUBSCRIBE:STREAM:TARGET_DEFINED",
	Response:               "RESPONSE",
	Other:                  "OTHER",
}

// String returns a string specifying type of gNMI Request.
func (m GNMIReqMode) String() string {
	if name, ok := gnmiReqModeToString[m]; ok {
		return name
	}
	return "?"
}

// TeX returns TeX-friendly version of a string.
func TeX(s string) string {
	return strings.Replace(strings.Replace(s, "_", "\\_", -1), "$", "", -1)
}

func newPath(p *protobufFieldGNMIPath, s *Instruction, m GNMIReqMode) *Path {
	return &Path{
		Path:      p.String(),
		Protobuf:  p,
		Scenarios: map[GNMIReqMode]map[string]bool{m: {s.Name: true}},
	}
}

// DOM contains information extracted from the input CDLang files.
type DOM struct {
	GlobalInst        map[string]*Instruction            `json:",omitempty"`
	Scenarios         map[string]*Instruction            `json:",omitempty"`
	SubScenarios      map[string]*Instruction            `json:",omitempty"`
	OtherScenarios    map[string]map[string]*Instruction `json:",omitempty"`
	OtherSubScenarios map[string]map[string]*Instruction `json:",omitempty"`
	CoveredPaths      map[string]*Path                   `json:",omitempty"`
}

// NewDOM returns initialized instance of DOM.
func NewDOM() *DOM {
	return &DOM{
		GlobalInst:        map[string]*Instruction{},
		Scenarios:         map[string]*Instruction{},
		SubScenarios:      map[string]*Instruction{},
		OtherScenarios:    map[string]map[string]*Instruction{},
		OtherSubScenarios: map[string]map[string]*Instruction{},
		CoveredPaths:      map[string]*Path{},
	}
}

// AddProtoTypeToFuncNameMapping adds mapping between PROTO and gRPC method related to it.
func (d *DOM) AddProtoTypeToFuncNameMapping(proto, funcName string) error {
	if n, ok := protoNameToFuncName[proto]; ok && n != funcName {
		return fmt.Errorf("conflicting mapping from Proto to Function: '%s' vs. '%s'", n, funcName)
	}
	protoNameToFuncName[proto] = funcName
	return nil
}

// AddProtoTypeToNamespaceMapping adds mapping between PROTO and gRPC namespace related to it.
func (d *DOM) AddProtoTypeToNamespaceMapping(proto, namespace string) error {
	if n, ok := protoNameToNamespace[proto]; ok && n != namespace {
		return fmt.Errorf("conflicting mapping from Proto to Namespace: '%s' vs. '%s'", n, namespace)
	}
	protoNameToNamespace[proto] = namespace
	return nil
}

// PostProcess augments the DOM object after the visitor is done.
// It verifies that the tree is consistent and adds information that could not be
// added during the visitor run.
func (d *DOM) PostProcess(ver *Version, logLevel int) {
	if logLevel != 1 {
		log.Printf("Selecting scenarios compliant with version: %s", ver)
	}
	d.Accept(newFilterScenariosByVersionVisitor(d, ver))
	if logLevel == 1 {
		log.Printf("Adding var declarations.")
	}
	d.Accept(newAddVarDeclVisitor())
	if logLevel == 1 {
		log.Printf("Adding IDs.")
	}
	d.Accept(newSetIDVisitor())
	if logLevel == 1 {
		log.Printf("Adding references to receive instructions.")
	}
	d.Accept(newAddNextInstRefVisitor())
	if logLevel == 1 {
		log.Printf("Grouping messages in AnyOrder group.")
	}
	d.Accept(newUpdateAnyOrderVisitor())
	if logLevel == 1 {
		log.Printf("Adding information about covered paths.")
	}
	d.Accept(newCoveredPathsVisitor(d))
}

// Marshal returns a JSON formatted string with the contents of DOM.
func (d *DOM) Marshal() (string, error) {
	b, err := json.Marshal(d)
	return string(b), err
}
