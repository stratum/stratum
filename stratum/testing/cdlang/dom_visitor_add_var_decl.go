// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

import "fmt"

// Implementation of a DOM visitor that adds instruction that declares a
// variable that is then set by one of the following instructions.

type addVarDeclVisitor struct {
	DOMVisitor
}

func newAddVarDeclVisitor() DOMVisitor {
	return &addVarDeclVisitor{}
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (v *addVarDeclVisitor) Accept(d *DOM) {
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
func (*addVarDeclVisitor) VisitScenario(i *Instruction) interface{} {
	// For each variable used by instructions add an instruction with declaration.
	for _, v := range i.Vars() {
		if !v.Ignore() {
			i.Children = append([]*Instruction{newVarDeclaration(v)}, i.Children...)
		}
	}
	return nil
}

// VisitSubScenario is called for each sub-scenario.
func (*addVarDeclVisitor) VisitSubScenario(i *Instruction) interface{} {
	// For each variable used by instructions add an instruction with declaration.
	for _, v := range i.Vars() {
		if !v.Ignore() {
			i.Children = append([]*Instruction{newVarDeclaration(v)}, i.Children...)
		}
	}
	return nil
}
