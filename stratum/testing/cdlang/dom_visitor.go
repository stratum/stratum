// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

// This file defines a DOM visitor interface that is used to implement visitors
// that augment the content of the DOM.

// The DOM has two lists of sets of instructions, namely Scenarios and SubScenarios.
// Each of those sets contains instructions in the order defined by the CDLang program.
// There are simple instructions like Send, Receive or Call, and there are grouping
// instructions like AtLeastOnce or AnyOrder that contain both simple instructions
// and other grouping instructions.
// All types of nodes in such tree-like structure have dedicated visiting functions
// that are defined by the DOMVisitor interface.

// DOMVisitor is an interface that has to be implemented by all DOM visitors.
type DOMVisitor interface {
	// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
	Accept(d *DOM)
	// VisitScenario is called for each scenario.
	VisitScenario(i *Instruction) interface{}
	// VisitSubScenario is called once for each sub-scenario.
	VisitSubScenario(i *Instruction) interface{}
	// VisitCall is called for each gNMI call instruction.
	VisitCall(i *Instruction) interface{}
	// VisitOpenStr is called for each open-stream instruction.
	VisitOpenStr(i *Instruction) interface{}
	// VisitCloseStr is called for each close-stream instruction.
	VisitCloseStr(i *Instruction) interface{}
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
	v.Accept(d)
}

func (i *Instruction) accept(v DOMVisitor) interface{} {
	switch i.Type {
	case ScenarioInst:
		return v.VisitScenario(i)
	case SubScenarioInst:
		return v.VisitSubScenario(i)
	case CallInst:
		return v.VisitCall(i)
	case OpenGNMIStrInst:
		return v.VisitOpenStr(i)
	case OpenCTRLStrInst:
		return v.VisitOpenStr(i)
	case CloseStrInst:
		return v.VisitCloseStr(i)
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
	if i.Children != nil {
		for _, c := range i.Children {
			if err, ok := c.accept(v).(error); ok {
				return err
			}
		}
	}
	return nil
}
