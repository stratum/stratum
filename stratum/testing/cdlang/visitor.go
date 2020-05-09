// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

// This file implements the visitor design pattern to extract information from
// the abstract syntax tree created by the parser. The extracted information
// is saved in the document object model.

import (
	"fmt"
	"strconv"
	"strings"

	"cdlang"

	"github.com/antlr/antlr4/runtime/Go/antlr"
)

// Visitor implements the visitor pattern (https://en.wikipedia.org/wiki/Visitor_pattern)
// to process the syntax tree produced by antlr, extract useful information from
// it and build a DOM object.
type Visitor struct {
	*antlr.BaseParseTreeVisitor
	*cdlang.BaseCDLangVisitor

	// The Document Object Model object. This is the place where the visitor
	// is putting information collected from the input Abstract Syntax Tree.
	dom *DOM

	// Implementation of a stack of Instruction elements.
	instructions []*Instruction
	// Implementation of a stack of instructionGroup elements.
	instructionGroups []instructionGroup
	// Implementation of a stack of protobufFieldGroup elements.
	protobufFieldGroups []protobufFieldGroup
	// Implementation of a stack of withParam elements.
	withParams []withParams

	// While building a gNMI path a pointer to the object is kept here to allow
	// elements to be added by independent calls to VisitGnmiPathElem().
	currPath *gnmiPath

	// Keeps track of open gRPC streams to detect situation when a not open stream is used.
	openStreams map[string]bool

	// Known gRPC domain.method combinations that result in opening a stream.
	streamOpeners map[string]map[string]func(string) *Instruction
}

// NewVisitor creates a new visitor instance.
func NewVisitor(dom *DOM) *Visitor {
	return &Visitor{
		BaseParseTreeVisitor: &antlr.BaseParseTreeVisitor{},
		BaseCDLangVisitor:    &cdlang.BaseCDLangVisitor{},
		dom:                  dom,
		openStreams:          map[string]bool{},
		streamOpeners: map[string]map[string]func(string) *Instruction{
			"gNMI": map[string]func(string) *Instruction{
				"Subscribe": newOpenGNMIStr,
			},
			"ctrl": map[string]func(string) *Instruction{
				"Execute": newOpenCTRLStr,
			},
		},
	}
}

// peekInst returns instruction that is at the top of the stack
// (or nil if the stack is empty)
func (v *Visitor) peekInst() *Instruction {
	if len(v.instructions) == 0 {
		return nil
	}
	return v.instructions[len(v.instructions)-1]
}

// popInst removes an element from the top of the stack.
func (v *Visitor) popInst() {
	v.instructions = v.instructions[:len(v.instructions)-1]
}

// push puts an instruction on top of the stack.
func (v *Visitor) pushInst(inst *Instruction) {
	v.instructions = append(v.instructions, inst)
}

// peekInstGroup returns instruction group that is at the top of the stack
// (or nil if the stack is empty)
func (v *Visitor) peekInstGroup() instructionGroup {
	if len(v.instructionGroups) == 0 {
		return nil
	}
	return v.instructionGroups[len(v.instructionGroups)-1]
}

// popInstGroup removes an element from the top of the stack.
func (v *Visitor) popInstGroup() {
	v.instructionGroups = v.instructionGroups[:len(v.instructionGroups)-1]
}

// push puts an instruction on top of the stack.
func (v *Visitor) pushInstGroup(group instructionGroup) {
	v.instructionGroups = append(v.instructionGroups, group)
}

// peekProtobufFieldGroup returns protobuf field group that is at the top of the stack
// (or nil if the stack is empty)
func (v *Visitor) peekProtobufFieldGroup() protobufFieldGroup {
	if len(v.protobufFieldGroups) == 0 {
		return nil
	}
	return v.protobufFieldGroups[len(v.protobufFieldGroups)-1]
}

// popProtobufFieldGroup removes an element from the top of the stack.
func (v *Visitor) popProtobufFieldGroup() {
	v.protobufFieldGroups = v.protobufFieldGroups[:len(v.protobufFieldGroups)-1]
}

// pushProtobufFieldGroup puts a protobuf field group on top of the stack.
func (v *Visitor) pushProtobufFieldGroup(group protobufFieldGroup) {
	v.protobufFieldGroups = append(v.protobufFieldGroups, group)
}

// peekProtobufField returns protobuf field that is at the top of the stack
// (or nil if the stack is empty)
func (v *Visitor) peekProtobufField() protobufField {
	if pg := v.peekProtobufFieldGroup(); pg != nil {
		return pg.LastField()
	}
	return nil
}

// peekWithParam returns instruction that accepts run-time parameters that is
// at the top of the stack (or nil if the stack is empty)
func (v *Visitor) peekWithParam() withParams {
	if len(v.withParams) == 0 {
		return nil
	}
	return v.withParams[len(v.withParams)-1]
}

// popWithParam removes an element from the top of the stack.
func (v *Visitor) popWithParam() {
	v.withParams = v.withParams[:len(v.withParams)-1]
}

// pushWithParam puts an instruction that accepts run-time parameters
// on top of the stack.
func (v *Visitor) pushWithParam(group withParams) {
	v.withParams = append(v.withParams, group)
}

// VisitContract handles 'contract' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitContract(ctx *cdlang.ContractContext) interface{} {
	return v.VisitChildren(ctx)
}

func (v *Visitor) openStreamNames() string {
	os := make([]string, 0, len(v.openStreams))
	for s := range v.openStreams {
		os = append(os, s)
	}
	return strings.Join(os, ", ")
}

// VisitConstProto handles 'constProto' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitConstProto(ctx *cdlang.ConstProtoContext) interface{} {
	v.pushInst(newConstProto())
	defer v.popInst()
	result := v.VisitChildren(ctx)
	n := ctx.GetN().GetText()
	if _, ok := v.dom.GlobalInst[n]; ok {
		return fmt.Errorf("const proto '%s' already defined", n)
	}
	v.dom.GlobalInst[n] = v.peekInst()
	return result
}

// VisitMapping handles 'mapping' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitMapping(ctx *cdlang.MappingContext) interface{} {
	req := ctx.GetReq().GetText()
	fun := ctx.GetF().GetText()
	resp := ctx.GetResp().GetText()
	ns := ctx.GetFd().GetText()
	if err := v.dom.AddProtoTypeToFuncNameMapping(req, fun); err != nil {
		return err
	}
	if err := v.dom.AddProtoTypeToFuncNameMapping(resp, fun); err != nil {
		return err
	}
	if err := v.dom.AddProtoTypeToNamespaceMapping(req, ns); err != nil {
		return err
	}
	if err := v.dom.AddProtoTypeToNamespaceMapping(resp, ns); err != nil {
		return err
	}
	return v.VisitChildren(ctx)
}

// VisitScenario handles 'scenario' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitScenario(ctx *cdlang.ScenarioContext) interface{} {
	s := newScenario(ctx.GetName().GetText())
	sos := ctx.GetSos().GetStart()
	if ctx.GetDisabled() != nil {
		s.Disabled = true
		sos = ctx.GetDisabled().GetStart()
	}
	s.Source = ctx.GetSos().GetInputStream().GetText(sos, ctx.GetEos().GetStop())
	v.pushInstGroup(s)
	defer v.popInstGroup()
	if result := v.VisitChildren(ctx); result != nil {
		return result
	}
	if len(v.openStreams) != 0 {
		return fmt.Errorf("scenario '%s': missing stream close for '%s'", s.Name, v.openStreamNames())
	}
	if _, ok := v.dom.OtherScenarios[s.Name][s.Version.String()]; ok {
		return fmt.Errorf("scenario '%s' version %s is already defined", s.Name, s.Version)
	}
	if _, ok := v.dom.OtherScenarios[s.Name]; !ok {
		v.dom.OtherScenarios[s.Name] = map[string]*Instruction{}
	}
	v.dom.OtherScenarios[s.Name][s.Version.String()] = s
	return nil
}

// VisitSubScenario handles 'subScenario' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitSubScenario(ctx *cdlang.SubScenarioContext) interface{} {
	s := newSubScenario(ctx.GetName().GetText())
	s.Source = ctx.GetSos().GetInputStream().GetText(ctx.GetSos().GetStart(), ctx.GetEos().GetStop())
	v.pushInstGroup(s)
	defer v.popInstGroup()
	if result := v.VisitChildren(ctx); result != nil {
		return result
	}
	if len(v.openStreams) != 0 {
		return fmt.Errorf("sub-scenario '%s': missing stream close for '%s'", s.Name, v.openStreamNames())
	}
	if _, ok := v.dom.OtherSubScenarios[s.Name][s.Version.String()]; ok {
		return fmt.Errorf("sub-scenario '%s' version %s is already defined", s.Name, s.Version)
	}
	if _, ok := v.dom.OtherSubScenarios[s.Name]; !ok {
		v.dom.OtherSubScenarios[s.Name] = map[string]*Instruction{}
	}
	v.dom.OtherSubScenarios[s.Name][s.Version.String()] = s
	return nil
}

// VisitVariableDeclaration handles 'variableDeclaration' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitVariableDeclaration(ctx *cdlang.VariableDeclarationContext) interface{} {
	if wp, ok := v.peekInstGroup().(withParams); ok {
		wp.addParam(newDeclaration(ctx.GetName().GetText(), ctx.GetType_name().GetText()))
	}
	return v.VisitChildren(ctx)
}

// VisitVariable handles 'variable' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitVariable(ctx *cdlang.VariableContext) interface{} {
	switch {
	case ctx.GetStr_name() != nil:
		v.peekWithParam().addParam(newVariable(ctx.GetStr_name().GetText(), "string"))
	case ctx.GetNum_name() != nil:
		v.peekWithParam().addParam(newVariable(ctx.GetNum_name().GetText(), "int64"))
	case ctx.GetSkiped() != nil:
		// We've got a value that we do not care about.
		v.peekWithParam().addParam(newIgnoredValue())
	default:
		return fmt.Errorf("unsupported variable type")
	}
	return v.VisitChildren(ctx)
}

// VisitConstant handles 'constant' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitConstant(ctx *cdlang.ConstantContext) interface{} {
	v.peekWithParam().addParam(newConstant(ctx.GetStr().GetText(), "string"))
	return v.VisitChildren(ctx)
}

// VisitVersion handles 'version' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitVersion(ctx *cdlang.VersionContext) interface{} {
	major, err := strconv.ParseInt(ctx.GetMajor().GetText(), 10, 64)
	if err != nil {
		return fmt.Errorf("error: major version is not an int (%v: %q)", err, ctx.GetMajor().GetText())
	}
	minor, err := strconv.ParseInt(ctx.GetMinor().GetText(), 10, 64)
	if err != nil {
		return fmt.Errorf("error: minor version is not an int (%v: %q)", err, ctx.GetMinor().GetText())
	}
	patch, err := strconv.ParseInt(ctx.GetPatch().GetText(), 10, 64)
	if err != nil {
		return fmt.Errorf("error: patch version is not an int (%v: %q)", err, ctx.GetPatch().GetText())
	}
	if ver, ok := v.peekInstGroup().(versioned); ok {
		ver.setVersion(major, minor, patch)
	}
	return v.VisitChildren(ctx)
}

// VisitInstruction handles 'instruction' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitInstruction(ctx *cdlang.InstructionContext) interface{} {
	return v.VisitChildren(ctx)
}

// VisitCall handles 'call' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCall(ctx *cdlang.CallContext) interface{} {
	v.pushInst(newCall("Call"))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitCallResponse handles 'callResponse' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCallResponse(ctx *cdlang.CallResponseContext) interface{} {
	// VisitProtobuf will override Protobuf field that points to the request.
	// Save it here, before calling VisitChildren, so it can be restored.
	callInst := v.peekInst()
	request := callInst.Protobuf
	callInst.Protobuf = nil
	result := v.VisitChildren(ctx)
	switch {
	case ctx.GetOk() != nil:
		// Protobuf field points to the response; move the response to correct field.
		callInst.Response = callInst.Protobuf
	case ctx.GetErr() != nil:
		callInst.ErrorExpected = true
	default:
		return fmt.Errorf("unsupported gNMI GET/SET status")
	}
	// Make Protobuf to point to the request (again).
	callInst.Protobuf = request
	return result
}

// VisitOpenStream handles 'open_stream' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitOpenStream(ctx *cdlang.OpenStreamContext) interface{} {
	domain := ctx.GetDomain().GetText()
	d, ok := v.streamOpeners[domain]
	if !ok {
		return fmt.Errorf("unknown gRPC domain '%s'", domain)
	}
	method := ctx.GetMethod().GetText()
	m, ok := d[method]
	if !ok {
		return fmt.Errorf("unknown gRPC method '%s.%s'", domain, method)
	}
	n := ctx.GetStream().GetText()
	if v.openStreams[n] {
		return fmt.Errorf("gRPC stream '%s' is already open", n)
	}
	v.openStreams[n] = true
	v.pushInst(m(n))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitCloseStream handles 'close_stream' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCloseStream(ctx *cdlang.CloseStreamContext) interface{} {
	n := ctx.GetStream().GetText()
	if !v.openStreams[n] {
		return fmt.Errorf("gRPC stream '%s' is already closed", n)
	}
	delete(v.openStreams, n)
	v.pushInst(newCloseStr("CloseStream", ctx.GetStream().GetText()))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitSend handles 'send' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitSend(ctx *cdlang.SendContext) interface{} {
	n := ctx.GetCh().GetText()
	if !v.openStreams[n] {
		return fmt.Errorf("gRPC stream '%s' is not open", n)
	}
	v.pushInst(newSend("Send", n))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitReceive handles 'receive' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitReceive(ctx *cdlang.ReceiveContext) interface{} {
	n := ctx.GetCh().GetText()
	if !v.openStreams[n] {
		return fmt.Errorf("gRPC stream '%s' is not open", n)
	}
	v.pushInst(newReceive("Receive", n))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitGroup handles 'group' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitGroup(ctx *cdlang.GroupContext) interface{} {
	var group interface{}
	switch {
	case ctx.GetAt_least_once() != nil:
		group = newGroupAtLeastOnce()
	case ctx.GetAny_order() != nil:
		group = newGroupAnyOrder()
	case ctx.GetZero_or_more() != nil:
		group = newGroupZeroOrMore()
	default:
		return fmt.Errorf("unsupported group element")
	}
	v.pushInstGroup(group.(instructionGroup))
	result := v.VisitChildren(ctx)
	v.popInstGroup()
	v.peekInstGroup().addInstruction(group.(*Instruction))
	return result
}

// VisitExecute handles 'execute' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitExecute(ctx *cdlang.ExecuteContext) interface{} {
	e := newExecute(ctx.GetName().GetText())
	v.pushInst(e)
	defer v.popInst()
	v.pushWithParam(e)
	defer v.popWithParam()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(e)
	return result
}

// VisitProtobuf handles 'protobuf' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobuf(ctx *cdlang.ProtobufContext) interface{} {
	proto := newProtobuf(ctx.GetName().GetText())
	v.pushProtobufFieldGroup(proto)
	defer v.popProtobufFieldGroup()
	result := v.VisitChildren(ctx)
	if t := v.peekInst().Type; t == SendInst || t == ReceiveInst || t == CallInst {
		v.peekInst().Protobuf = proto
	}
	return result
}

// VisitProtobufField handles 'protobufField' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobufField(ctx *cdlang.ProtobufFieldContext) interface{} {
	return v.VisitChildren(ctx)
}

// VisitProtobufFieldSimple handles 'protobufFieldSimple' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobufFieldSimple(ctx *cdlang.ProtobufFieldSimpleContext) interface{} {
	switch {
	case ctx.GetVal_number() != nil:
		v.peekProtobufFieldGroup().addField(newProtobufFieldInt())
		if ok := v.peekProtobufField().setValue(ctx.GetVal_number().GetText()); !ok {
			return fmt.Errorf("error: wrong field value (not an int): %q", ctx.GetVal_number().GetText())
		}
	case ctx.GetVal_string() != nil:
		v.peekProtobufFieldGroup().addField(newProtobufFieldString())
		str, err := strconv.Unquote(ctx.GetVal_string().GetText())
		if err != nil {
			return fmt.Errorf("error: wrong field value (not a string): %q", ctx.GetVal_string().GetText())
		}
		if ok := v.peekProtobufField().setValue(str); !ok {
			return fmt.Errorf("error: wrong field value (not a string): %q", ctx.GetVal_string().GetText())
		}
	case ctx.GetVal_enum() != nil:
		v.peekProtobufFieldGroup().addField(newProtobufFieldEnum())
		if ok := v.peekProtobufField().setValue(ctx.GetVal_enum().GetText()); !ok {
			return fmt.Errorf("error: wrong field value (not an enum): %q", ctx.GetVal_enum().GetText())
		}
	case ctx.GetVal_path() != nil:
		v.peekProtobufFieldGroup().addField(newProtobufFieldGnmiPath())
	case ctx.GetVal_var() != nil:
		f := newProtobufFieldVariable()
		// Value will be set by VisitVariable().
		v.peekProtobufFieldGroup().addField(f)
		v.pushWithParam(f)
		defer v.popWithParam()
	default:
		return fmt.Errorf("error: unknown field value type")
	}
	v.peekProtobufField().setName(ctx.GetName().GetText())
	return v.VisitChildren(ctx)
}

// VisitProtobufFieldGroup handles 'protobufFieldGroup' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobufFieldGroup(ctx *cdlang.ProtobufFieldGroupContext) interface{} {
	var group *protobufFieldSequence
	if ctx.GetCast() != nil {
		group = newProtobufFieldSequence(true)
		group.CastType = ctx.GetCast().GetText()
	} else {
		group = newProtobufFieldSequence(false)
	}
	v.pushProtobufFieldGroup(group)
	result := v.VisitChildren(ctx)
	v.popProtobufFieldGroup()
	group.setName(ctx.GetName().GetText())
	v.peekProtobufFieldGroup().addField(group)
	return result
}

// VisitProtobufFieldRepeated handles 'protobufFieldRepeated' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobufFieldRepeated(ctx *cdlang.ProtobufFieldRepeatedContext) interface{} {
	array := newProtobufFieldRepeated()
	v.pushProtobufFieldGroup(array)
	result := v.VisitChildren(ctx)
	v.popProtobufFieldGroup()
	v.peekProtobufFieldGroup().addField(array)
	array.setName(ctx.GetName().GetText())
	// Each row of the array has to have the same name as the array.
	for _, row := range array.Fields {
		row.setName(ctx.GetName().GetText())
	}
	return result
}

// VisitProtobufFieldRepeatedRow handles 'protobufFieldRepeatedRow' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitProtobufFieldRepeatedRow(ctx *cdlang.ProtobufFieldRepeatedRowContext) interface{} {
	row := newProtobufFieldRepeatedRow()
	switch {
	case ctx.GetZero_or_more() != nil:
		row.SequenceSize = protobufFieldSequenceSizeZeroOrMore
	case ctx.GetOne_or_more() != nil:
		row.SequenceSize = protobufFieldSequenceSizeOneOrMore
	case ctx.GetZero_or_one() != nil:
		row.SequenceSize = protobufFieldSequenceSizeZeroOrOne
	}
	v.pushProtobufFieldGroup(row)
	result := v.VisitChildren(ctx)
	v.popProtobufFieldGroup()
	v.peekProtobufFieldGroup().addField(row)
	return result
}

// VisitPath handles 'path' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitPath(ctx *cdlang.PathContext) interface{} {
	v.currPath = newGNMIPath()
	result := v.VisitChildren(ctx)
	if ok := v.peekProtobufField().setValue(v.currPath); !ok {
		return fmt.Errorf("error: wrong field value (not a path): %v", v.currPath)
	}
	v.currPath = nil
	return result
}

// VisitPathElement handles 'pathElement' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitPathElement(ctx *cdlang.PathElementContext) interface{} {
	switch {
	case ctx.GetName() != nil:
		switch {
		case ctx.GetA() != nil:
			v.currPath.addElemKeyValue(ctx.GetName().GetText(), ctx.GetKey().GetText(), newStringConstant(ctx.GetA().GetText()))
		case ctx.GetRd_var() != nil || ctx.GetWr_var() != nil:
			v.pushWithParam(newProtobufFieldVariable())
			defer func() {
				if e, ok := v.peekWithParam().(*protobufFieldVariable); ok {
					if ctx.GetWr_var() != nil {
						e.Param().ParameterKind = variableDecl
					}
					v.currPath.addElemKeyValue(ctx.GetName().GetText(), ctx.GetKey().GetText(), e.Param())
				}
				v.popWithParam()
			}()
		case ctx.GetE() != nil:
			v.currPath.addElemKeyValue(ctx.GetName().GetText(), ctx.GetKey().GetText(), newConstant(ctx.GetE().GetText(), "string"))
		default:
			v.currPath.addElem(ctx.GetName().GetText())
		}
	case ctx.GetParam() != nil:
		v.pushWithParam(newGNMIPathElem())
		defer func() {
			v.currPath.addExistingElem(v.peekWithParam().(*gnmiPathElem))
			v.popWithParam()
		}()
	}
	return v.VisitChildren(ctx)
}

// VisitChildren visits children nodes in the parser tree.
// TODO(tmadejski): remove VisitChildren when antlr4 Go has such functionality internaly supported.
func (v *Visitor) VisitChildren(node antlr.RuleNode) interface{} {
	for _, child := range node.GetChildren() {
		if p, ok := child.(antlr.ParseTree); ok {
			if err, ok := p.Accept(v).(error); ok {
				return err
			}
		}
	}
	return nil
}
