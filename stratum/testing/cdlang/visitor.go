package cdl

// This file implements the visitor design pattern to extract information from
// the abstract syntax tree created by the parser. The extracted information
// is saved in the document object model.

import (
	"fmt"
	"strconv"

	"google3/platforms/networking/hercules/testing/cdlang/cdlang"
	"google3/golang/antlr4/antlr"
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
}

// NewVisitor creates a new visitor instance.
func NewVisitor(dom *DOM) *Visitor {
	return &Visitor{
		BaseParseTreeVisitor: &antlr.BaseParseTreeVisitor{},
		BaseCDLangVisitor:    &cdlang.BaseCDLangVisitor{},
		dom:                  dom,
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

// VisitScenario handles 'scenario' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitScenario(ctx *cdlang.ScenarioContext) interface{} {
	s := newScenario(ctx.GetName().GetText())
	v.dom.Scenarios[ctx.GetName().GetText()] = s
	v.pushInstGroup(s)
	defer v.popInstGroup()
	return v.VisitChildren(ctx)
}

// VisitSubScenario handles 'subScenario' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitSubScenario(ctx *cdlang.SubScenarioContext) interface{} {
	s := newSubScenario(ctx.GetName().GetText())
	v.dom.SubScenarios[ctx.GetName().GetText()] = s
	v.pushInstGroup(s)
	defer v.popInstGroup()
	return v.VisitChildren(ctx)
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

// VisitSend handles 'send' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitSend(ctx *cdlang.SendContext) interface{} {
	v.pushInst(newSend("Send", ctx.GetCh().GetText()))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitReceive handles 'receive' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitReceive(ctx *cdlang.ReceiveContext) interface{} {
	v.pushInst(newReceive("Receive", ctx.GetCh().GetText()))
	defer v.popInst()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(v.peekInst())
	return result
}

// VisitCheck handles 'check' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCheck(ctx *cdlang.CheckContext) interface{} {
	return v.VisitChildren(ctx)
}

// VisitCheckRegex handles 'checkRegex' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCheckRegex(ctx *cdlang.CheckRegexContext) interface{} {
	c := newCheckRegExMatch("CheckRegExMatch")
	v.pushInst(c)
	defer v.popInst()
	v.pushWithParam(c)
	defer v.popWithParam()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(c)
	return result
}

// VisitCheckUnique handles 'checkUnique' element of CDLang grammar. Returns error detected by children of this element.
func (v *Visitor) VisitCheckUnique(ctx *cdlang.CheckUniqueContext) interface{} {
	c := newCheckUnique("CheckUnique")
	v.pushInst(c)
	defer v.popInst()
	v.pushWithParam(c)
	defer v.popWithParam()
	result := v.VisitChildren(ctx)
	v.peekInstGroup().addInstruction(c)
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
	if t := v.peekInst().Type; t == SendInst || t == ReceiveInst {
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
	group := newProtobufFieldSequence()
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
		case ctx.GetV() != nil:
			v.pushWithParam(newProtobufFieldVariable())
			defer func() {
				if e, ok := v.peekWithParam().(*protobufFieldVariable); ok {
					v.currPath.addElemKeyValue(ctx.GetName().GetText(), "name", e.Param())
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
		defer v.popWithParam()
	}
	return v.VisitChildren(ctx)
}

// VisitChildren visits children nodes in the parser tree.
// TODO(tmadejski): remove VisitChildren when antlr4 Go has such functionality internaly supported.
func (v *Visitor) VisitChildren(node antlr.RuleNode) interface{} {
	var result interface{}
	for _, child := range node.GetChildren() {
		if p, ok := child.(antlr.ParseTree); ok {
			result = p.Accept(v)
		}
	}
	return result
}
