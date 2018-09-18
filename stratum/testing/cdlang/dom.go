// Package cdl provides internals of the CDLang transpiler.
package cdl

// This file implements document object model - an object that is used to store
// information extracted from the input files written in CDLang and which is
// passed as an input to the Go template engine.

import (
	"encoding/json"
	"fmt"
	"strconv"
	"strings"

	"google3/base/go/log"
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
	// CheckRegexMatchInst indicates a check regex instruction.
	CheckRegexMatchInst
	// CheckUniqueInst indicates a check unique instruction.
	CheckUniqueInst
	// VarDeclarationInst indicates a instruction declaring a variable.
	VarDeclarationInst
	// ScenarioInst indicates a sceanario instruction.
	ScenarioInst
	// SubScenarioInst indicates a sub-sceanrtio instruction.
	SubScenarioInst
)

var instTypeToString = map[InstType]string{
	SendInst:             "send",
	ReceiveInst:          "receive",
	ExecuteInst:          "execute",
	GroupZeroOrMoreInst:  "zero_or_more",
	GroupAtLeastOnceInst: "at_least_once",
	GroupAnyOrderInst:    "any_order",
	CheckRegexMatchInst:  "check_regex_match",
	CheckUniqueInst:      "check_uniqe",
	VarDeclarationInst:   "var_declaration",
	ScenarioInst:         "scenario",
	SubScenarioInst:      "sub_scenario",
}

// String returns a string specifying type of instruction. Used for debugging.
func (s *InstType) String() string {
	if name, ok := instTypeToString[*s]; ok {
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
	// Valid for Send and Receive only.
	Protobuf *protobuf // Protobuf to be sent or received.
	Channel  string    // Either 'gnmi' or 'ctrl'.
	// Valid for receive only.
	Next *Instruction // Another message that can be recived at this point.
	Skip bool         // This instruction is received in preciding loop.
	// Valid for varDeclaration only.
	Variable *Param
	// Valid for execute and check* only.
	Params []*Param
	// Instructions that are frouped by this instruction.
	Children []*Instruction
	// Valid for sceanrio and sub-scenario only
	Version *Version
	// Valid for AnyOrder group only.
	GNMI []*Instruction
	CTRL []*Instruction
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

// Returns initialized instance of checkRegExMatch.
func newCheckRegExMatch(name string) *Instruction {
	return &Instruction{
		Type: CheckRegexMatchInst,
		Name: name,
	}
}

// Returns initialized instance of checkUnique.
func newCheckUnique(name string) *Instruction {
	return &Instruction{
		Type: CheckUniqueInst,
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

// ParameterKind defines the type of the parameter.
// A CDLang instruction paramater can be either a variable or a constant.
type ParameterKind int

const (
	variable ParameterKind = iota
	stringConstant
	otherConstant
	declaration
	ignored
)

// Param is a CDLang instruction run-time paramater.
// Some instructions as `execute` or `check unique` accept parameters.
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
	case variable:
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
	Name string
	Key  map[string]*Param
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
	// TODO(tmadejski): add code that will decide in this element of path can be ignored.
	return false
}

func (p *gnmiPathElem) IsType(fieldType int) bool {
	// TODO(tmadejski): add code that will check if the field is or right type.
	return false
}

func (p *gnmiPathElem) addParam(param *Param) {
	log.Fatal("Wrong version of addParam() has been called!")
}

func (p *gnmiPathElem) Params() []*Param {
	result := []*Param{}
	for _, v := range p.Key {
		if v.ParameterKind == variable {
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
	Elem []*gnmiPathElem
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
)

type protobufField interface {
	setName(name string)
	setValue(val interface{}) bool
	IsSimple() bool
	IsRepeated() bool
	IsType(fieldType int) bool
	Ignore() bool
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

func (f *genericProtobufField) IsType(fieldType int) bool {
	return int(f.FieldType) == fieldType
}

func (f *genericProtobufField) Ignore() bool {
	return false
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

type protobufFieldVariable struct {
	*genericProtobufField
	Parameters []*Param
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

type protobufFieldGnmiPath struct {
	*genericProtobufField
	Value *gnmiPath
}

// Returns initialized instance of protobufFieldGnmiPath.
func newProtobufFieldGnmiPath() *protobufFieldGnmiPath {
	return &protobufFieldGnmiPath{
		genericProtobufField: newGenericProtobufField(gnmiPathField),
	}
}

func (f *protobufFieldGnmiPath) setValue(val interface{}) bool {
	if v, ok := val.(*gnmiPath); ok {
		f.Value = v
		return true
	}
	return false
}

func (f *protobufFieldGnmiPath) Sting() string {
	var result string
	for _, elem := range f.Value.Elem {
		result += "/" + elem.String()
		if elem.Key != nil {
			result += "["
			for key, val := range elem.Key {
				result = fmt.Sprint(result, key, "=", val)
			}
			result += "]"
		}
	}
	return result
}

func (f *protobufFieldGnmiPath) IsSimple() bool {
	return false
}

type protobufFieldGroup interface {
	addField(field protobufField)
	LastField() protobufField
	Vars() []*Param
}

// Used to represent a complex (as in: not-simple) protobuf field.
type protobufFieldSequence struct {
	*genericProtobufField
	Fields []protobufField
}

// Returns initialized instance of protobufFieldSequence.
func newProtobufFieldSequence() *protobufFieldSequence {
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

func (p *protobufFieldSequence) IsSimple() bool {
	return false
}

func (p *protobufFieldSequence) IsRepeated() bool {
	return p.FieldType == repeatedField
}

func (p *protobufFieldSequence) Vars() []*Param {
	result := []*Param{}
	for _, f := range p.Fields {
		switch v := f.(type) {
		case *protobufFieldVariable:
			result = append(result, v.Param())
		case *protobufFieldGnmiPath:
			if v := v.Value.Elem; v != nil {
				for _, e := range v {
					if v := e.Param(); v != nil {
						result = append(result, v)
					}
				}
			}
		case *protobufFieldSequence:
			result = append(result, v.Vars()...)
		}
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

// Used to represent a ProtoBuf as a whole entity.
type protobuf struct {
	*protobufFieldSequence
	TypeName string
}

// Returns initialized instance of Protobuf.
func newProtobuf(typeName string) *protobuf {
	return &protobuf{
		protobufFieldSequence: newProtobufFieldSequence(),
		TypeName:              typeName,
	}
}

// DOM contains information extracted from the input CDLang files.
type DOM struct {
	Scenarios    map[string]*Instruction
	SubScenarios map[string]*Instruction
}

// NewDOM returns initialized instance of DOM.
func NewDOM() *DOM {
	return &DOM{
		Scenarios:    map[string]*Instruction{},
		SubScenarios: map[string]*Instruction{},
	}
}

// PostProcess augments the DOM object after the visitor is done.
// It verifies that the tree is consistent and adds information that could not be
// added during the visitor run.
func (d *DOM) PostProcess() {
	fmt.Println("Adding var declarations.")
	d.Accept(newAddVarDeclVisitor())
	fmt.Println("Adding IDs.")
	d.Accept(newSetIDVisitor())
	fmt.Println("Adding references to receive instructions.")
	d.Accept(newAddNextInstRefVisitor())
	fmt.Println("Grouping messages in AnyOrder group.")
	d.Accept(newUpdateAnyOrderVisitor())
}

// Marshal returns a JSON formatted string with the contents of DOM.
func (d *DOM) Marshal() (string, error) {
	b, err := json.Marshal(d)
	return string(b), err
}
