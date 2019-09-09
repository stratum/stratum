//
// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
