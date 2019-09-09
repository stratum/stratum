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

// Implementation of a DOM visitor that augments the contents of the DOM by
// leaving only one version of each scenario.

type filterScenariosByVersionVisitor struct {
	DOMVisitor
	ver *Version
	d   *DOM
}

func newFilterScenariosByVersionVisitor(d *DOM, ver *Version) DOMVisitor {
	return &filterScenariosByVersionVisitor{
		ver: ver,
		d:   d,
	}
}

func (f *filterScenariosByVersionVisitor) find(all map[string]map[string]*Instruction, sel map[string]*Instruction) {
	for name, vers := range all {
		// For each scenario type:
		var max *Version
		for v, _ := range vers {
			// For each version of this scenario type:
			ver, err := NewVersion(v)
			if err != nil {
				continue
			}
			if ver.Compare(max) > 0 && ver.Compare(f.ver) <= 0 {
				max = ver
			}
		}
		if max == nil {
			// Required version of the scenario has not been found.
			continue
		}
		// Move the selected (sub)scenario into the list of (sub)scenarios to be used.
		sel[name] = all[name][max.String()]
		delete(all[name], max.String())
		if len(all[name]) == 0 {
			delete(all, name)
		}
	}
}

// Accept goes through all known scenarios and finds the one that compiles with requested version.
func (f *filterScenariosByVersionVisitor) Accept(d *DOM) {
	f.find(d.OtherScenarios, d.Scenarios)
	f.find(d.OtherSubScenarios, d.SubScenarios)
}
