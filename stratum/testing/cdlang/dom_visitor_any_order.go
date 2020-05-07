// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

import "fmt"

// Implementation of a DOM visitor that groups receive instruction by
// the channel used found in AnyOrder grouping instructions.
// This grouping helps with code generation.

type updateAnyOrderVisitor struct {
	DOMVisitor
}

func newUpdateAnyOrderVisitor() DOMVisitor {
	return &updateAnyOrderVisitor{}
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (v updateAnyOrderVisitor) Accept(d *DOM) {
	for _, s := range d.Scenarios {
		if err, ok := s.accept(v).(error); ok {
			fmt.Println(err)
		}
	}
	for _, s := range d.SubScenarios {
		if err, ok := s.accept(v).(error); ok {
			fmt.Println(err)
		}
	}
}

// VisitScenario is called for each scenario.
func (v updateAnyOrderVisitor) VisitScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSubScenario is called once for each sub-scenario.
func (v updateAnyOrderVisitor) VisitSubScenario(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
func (v updateAnyOrderVisitor) VisitGroupAtLeastOnce(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
func (v updateAnyOrderVisitor) VisitGroupZeroOrMore(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAnyOrder is called for each AnyOrder group.
func (v updateAnyOrderVisitor) VisitGroupAnyOrder(i *Instruction) interface{} {
	i.ChildrenPerChannel = map[string][]*Instruction{}
	for _, c := range i.Children {
		if c.Type != ReceiveInst {
			continue
		}
		i.ChildrenPerChannel[c.Channel] = append(i.ChildrenPerChannel[c.Channel], c)
	}
	return i.VisitChildren(v)
}

// VisitCall is called for each gNMI call instruction.
func (v updateAnyOrderVisitor) VisitCall(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitOpenStr is called for each open-stream instruction.
func (v updateAnyOrderVisitor) VisitOpenStr(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitCloseStr is called for each close-stream instruction.
func (v updateAnyOrderVisitor) VisitCloseStr(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSend is called for each send instruction.
func (v updateAnyOrderVisitor) VisitSend(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitReceive is called for each receive instruction.
func (v updateAnyOrderVisitor) VisitReceive(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitVarDeclaration is called for each varDeclaration instruction.
func (v updateAnyOrderVisitor) VisitVarDeclaration(i *Instruction) interface{} {
	return i.VisitChildren(v)
}
