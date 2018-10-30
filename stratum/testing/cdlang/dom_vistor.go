package cdl

// This file implements a number of DOM visitors that are used to augment the
// contents of the DOM.

import "fmt"

// DOMVisitor is an interface that has to be implemented by all DOM visitors.
type DOMVisitor interface {
	// VisitScenario is called for each scenario.
	VisitScenario(i *Instruction) interface{}
	// VisitSubScenario is called once for each sub-scenario.
	VisitSubScenario(i *Instruction) interface{}
	// VisitSend is called for each send instruction.
	VisitSend(i *Instruction) interface{}
	// VisitReceive is called for each receive instruction.
	VisitReceive(i *Instruction) interface{}
	// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
	VisitGroupAtLeastOnce(i *Instruction) interface{}
	// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
	VisitGroupZeroOrMore(i *Instruction) interface{}
	// VisitGroupAnyOrder is called for each GroupAnyOrder instruction group.
	VisitGroupAnyOrder(i *Instruction) interface{}
	// VisitVarDeclaration is called for each varDeclaration instruction.
	VisitVarDeclaration(i *Instruction) interface{}
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (d *DOM) Accept(v DOMVisitor) {
	for _, s := range d.Scenarios {
		v.VisitScenario(s)
	}
	for _, s := range d.SubScenarios {
		v.VisitSubScenario(s)
	}
}

func (i *Instruction) accept(v DOMVisitor) interface{} {
	switch i.Type {
	case ScenarioInst:
		return v.VisitScenario(i)
	case SubScenarioInst:
		return v.VisitSubScenario(i)
	case SendInst:
		return v.VisitSend(i)
	case ReceiveInst:
		return v.VisitReceive(i)
	case GroupAtLeastOnceInst:
		return v.VisitGroupAtLeastOnce(i)
	case GroupZeroOrMoreInst:
		return v.VisitGroupZeroOrMore(i)
	case GroupAnyOrderInst:
		return v.VisitGroupAnyOrder(i)
	case VarDeclarationInst:
		return v.VisitVarDeclaration(i)
	default:
		return nil
	}
}

// VisitChildren goes through all children of this instruction and calls visitor for each of them.
func (i *Instruction) VisitChildren(v DOMVisitor) interface{} {
	var result interface{}
	if i.Children != nil {
		for _, c := range i.Children {
			result = c.accept(v)
		}
	}
	return result
}

type baseDOMVisitor struct {
}

// NewBaseDOMVisitor returns initialized instance of baseDOMVisitor that implements all required methods.
func NewBaseDOMVisitor() DOMVisitor {
	return &baseDOMVisitor{}
}

// VisitScenario is called for each scenario.
func (v *baseDOMVisitor) VisitScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSubScenario is called once for each sub-scenario.
func (v *baseDOMVisitor) VisitSubScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSend is called for each send instruction.
func (v *baseDOMVisitor) VisitSend(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitReceive is called for each receive instruction.
func (v *baseDOMVisitor) VisitReceive(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
func (v *baseDOMVisitor) VisitGroupAtLeastOnce(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
func (v *baseDOMVisitor) VisitGroupZeroOrMore(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAnyOrder is called for each GroupAnyOrder instruction group.
func (v *baseDOMVisitor) VisitGroupAnyOrder(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitVarDeclaration is called for each varDeclaration instruction.
func (v *baseDOMVisitor) VisitVarDeclaration(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

type setIDVisitor struct {
	DOMVisitor
	id int32
}

func newSetIDVisitor() DOMVisitor {
	return &setIDVisitor{
		DOMVisitor: NewBaseDOMVisitor(),
		id:         0,
	}
}

// VisitScenario is called for each scenario.
func (v *setIDVisitor) VisitScenario(i *Instruction) interface{} {
	v.id = 0
	return i.VisitChildren(v)
}

// VisitSubScenario is called for each sub-scenario.
func (v *setIDVisitor) VisitSubScenario(i *Instruction) interface{} {
	v.id = 0
	return i.VisitChildren(v)
}

func (v *setIDVisitor) setID(i *Instruction) {
	i.ID = v.id
	v.id++
}

// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
func (v *setIDVisitor) VisitGroupAtLeastOnce(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
func (v *setIDVisitor) VisitGroupZeroOrMore(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitGroupAnyOrder is called for each GroupAnyOrder instruction group.
func (v *setIDVisitor) VisitGroupAnyOrder(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitSend is called for each send instruction.
func (v *setIDVisitor) VisitSend(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitReceive is called for each receive instruction.
func (v *setIDVisitor) VisitReceive(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitVarDeclaration is called for each varDeclaration instruction.
func (v *setIDVisitor) VisitVarDeclaration(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

type addVarDeclVisitor struct {
	DOMVisitor
}

func newAddVarDeclVisitor() DOMVisitor {
	return &addVarDeclVisitor{
		DOMVisitor: NewBaseDOMVisitor(),
	}
}

// VisitScenario is called for each scenario.
func (v *addVarDeclVisitor) VisitScenario(i *Instruction) interface{} {
	// For each variable used by instructions add an instruction with declaration.
	for _, v := range i.Vars() {
		if !v.Ignore() {
			i.Children = append([]*Instruction{newVarDeclaration(v)}, i.Children...)
		}
	}
	return i.VisitChildren(v)
}

// VisitSubScenario is called for each sub-scenario.
func (v *addVarDeclVisitor) VisitSubScenario(i *Instruction) interface{} {
	// For each variable used by instructions add an instruction with declaration.
	for _, v := range i.Vars() {
		if !v.Ignore() {
			i.Children = append([]*Instruction{newVarDeclaration(v)}, i.Children...)
		}
	}
	return i.VisitChildren(v)
}

type addNextInstRefVisitor struct {
	DOMVisitor
}

func newAddNextInstRefVisitor() DOMVisitor {
	return &addNextInstRefVisitor{
		DOMVisitor: NewBaseDOMVisitor(),
	}
}

func findBackToBackMsgs(i *Instruction) {
	fmt.Println(i.Name)
	for c := 0; c < len(i.Children)-1; c++ {
		g := i.Children[c]
		next := i.Children[c+1]
		if g.Children != nil && next.Type == ReceiveInst {
			// c is position of a group instruction and next instruction is a receive.
			fmt.Println(g.Name, c, "receive", c+1)
			for _, cc := range g.Children {
				if cc.Type == ReceiveInst && cc.Channel == next.Channel {
					fmt.Println("found back-to-back messages on", cc.Channel)
					cc.Next = next
					next.Skip = true
				}
			}
		}
	}
}

// VisitScenario is called for each scenario.
func (v *addNextInstRefVisitor) VisitScenario(i *Instruction) interface{} {
	findBackToBackMsgs(i)
	return nil
}

// VisitSubScenario is called for each sub-scenario.
func (v *addNextInstRefVisitor) VisitSubScenario(i *Instruction) interface{} {
	findBackToBackMsgs(i)
	return nil
}

type updateAnyOrderVisitor struct {
}

func newUpdateAnyOrderVisitor() DOMVisitor {
	return &updateAnyOrderVisitor{}
}

// VisitScenario is called for each scenario.
func (v *updateAnyOrderVisitor) VisitScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSubScenario is called once for each sub-scenario.
func (v *updateAnyOrderVisitor) VisitSubScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSend is called for each send instruction.
func (v *updateAnyOrderVisitor) VisitSend(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitReceive is called for each receive instruction.
func (v *updateAnyOrderVisitor) VisitReceive(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
func (v *updateAnyOrderVisitor) VisitGroupAtLeastOnce(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
func (v *updateAnyOrderVisitor) VisitGroupZeroOrMore(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAnyOrder is called for each AnyOrder group.
func (v *updateAnyOrderVisitor) VisitGroupAnyOrder(i *Instruction) interface{} {
	fmt.Println(i.Name)
	i.GNMI = []*Instruction{}
	i.CTRL = []*Instruction{}
	for _, c := range i.Children {
		if c.Type != ReceiveInst {
			continue
		}
		switch c.Channel {
		case "gnmi":
			i.GNMI = append(i.GNMI, c)
		case "ctrl":
			i.CTRL = append(i.CTRL, c)
		}
	}
	if len(i.GNMI) == 0 {
		i.GNMI = nil
	}
	if len(i.CTRL) == 0 {
		i.CTRL = nil
	}
	return i.VisitChildren(v)
}

// VisitVarDeclaration is called for each varDeclaration instruction.
func (v *updateAnyOrderVisitor) VisitVarDeclaration(i *Instruction) interface{} {
	return i.VisitChildren(v)
}
