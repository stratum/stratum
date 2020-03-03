// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

import "fmt"

// Implementation of a DOM visitor that augments the contents of the DOM by
// adding a sequential instruction ID to each instruction.

type setIDVisitor struct {
	id int32
}

func newSetIDVisitor() DOMVisitor {
	return &setIDVisitor{}
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (v *setIDVisitor) Accept(d *DOM) {
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

// VisitCall is called for each gNMI call instruction.
func (v *setIDVisitor) VisitCall(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitOpenStr is called for each open-stream instruction.
func (v *setIDVisitor) VisitOpenStr(i *Instruction) interface{} {
	v.setID(i)
	return i.VisitChildren(v)
}

// VisitCloseStr is called for each close-stream instruction.
func (v *setIDVisitor) VisitCloseStr(i *Instruction) interface{} {
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
