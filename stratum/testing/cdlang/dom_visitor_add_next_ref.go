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

// Implementation of a DOM visitor that for each ZeroOrMore and AtLeastOnce
// instruction grouping instruction adds link to the immediatelly following
// receive instruction. This information is needed to correctly generate code
// that checks when to exit the grouping instruction.

type addNextInstRefVisitor struct {
	DOMVisitor
}

func newAddNextInstRefVisitor() DOMVisitor {
	return &addNextInstRefVisitor{}
}

func findBackToBackMsgs(i *Instruction) {
	for c := 0; c < len(i.Children)-1; c++ {
		g := i.Children[c]
		// Only ZeroOrMore and AtLeastOnce require check for next receive.
		if g.Type != GroupZeroOrMoreInst && g.Type != GroupAtLeastOnceInst {
			continue
		}
		next := i.Children[c+1]
		if next.Type != ReceiveInst {
			continue
		}
		// c is position of a group instruction and next instruction is a receive.
		for _, cc := range g.Children {
			if cc.Type == ReceiveInst && cc.Channel == next.Channel && !next.Skip {
				cc.Next = next
				next.Skip = true
			}
		}
	}
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (v *addNextInstRefVisitor) Accept(d *DOM) {
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
func (*addNextInstRefVisitor) VisitScenario(i *Instruction) interface{} {
	findBackToBackMsgs(i)
	return nil
}

// VisitSubScenario is called for each sub-scenario.
func (*addNextInstRefVisitor) VisitSubScenario(i *Instruction) interface{} {
	findBackToBackMsgs(i)
	return nil
}
