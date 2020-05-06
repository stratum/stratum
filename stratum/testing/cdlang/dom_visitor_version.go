// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
