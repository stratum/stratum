// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

package cdl

import "fmt"

// Implementation of a DOM visitor that surveys the DOM and for each found
// YANG path collects information in which mode and by wich scenario the path
// is used.

type coveredPathsVisitor struct {
	paths    map[string]*Path
	scenario *Instruction
}

func newCoveredPathsVisitor(d *DOM) DOMVisitor {
	return &coveredPathsVisitor{
		paths: d.CoveredPaths,
	}
}

func (v *coveredPathsVisitor) addPathsFromField(f interface{}, m GNMIReqMode) {
	switch p := f.(type) {
	case []protobufField:
		for _, sf := range p {
			v.addPathsFromField(sf, m)
		}
	case *protobufFieldSequence:
		v.addPathsFromField(p.Fields, m)
	case *protobufFieldGNMIPath:
		path, ok := v.paths[p.String()]
		if !ok {
			v.paths[p.String()] = newPath(p, v.scenario, m)
			return
		}
		scen, ok := path.Scenarios[m]
		if !ok {
			scen = map[string]bool{}
			path.Scenarios[m] = scen
		}
		scen[v.scenario.Name] = true
	}
}

func (v *coveredPathsVisitor) getFieldMode(p protobufField) GNMIReqMode {
	f, ok := p.(*protobufFieldEnum)
	if !ok {
		return Other
	}
	switch f.Value {
	case "ONCE":
		return SubscribeOnce
	case "POLL":
		return SubscribePoll
	case "SAMPLE":
		return SubscribeSample
	case "ON_CHANGE":
		return SubscribeOnChange
	case "TARGET_DEFINED":
		return SubscribeTargetDefined
	default:
		return Other
	}
}

func (v *coveredPathsVisitor) addPaths(p *protobuf) error {
	switch p.TypeName {
	case "GetRequest":
		v.addPathsFromField(p.Fields, Get)
	case "GetResponse", "SetResponse", "SubscribeResponse":
		v.addPathsFromField(p.Fields, Response)
	case "SetRequest":
		v.addPathsFromField(p.Fields, Set)
	case "SubscribeRequest":
		for _, f := range p.Fields {
			ff, ok := f.(*protobufFieldSequence)
			if !ok || ff.Name != "subscribe" {
				continue
			}
			for _, s := range ff.Fields {
				ss, ok := s.(*protobufFieldEnum)
				if !ok || ss.Name != "mode" {
					continue
				}
				if m := v.getFieldMode(ss); m == SubscribeOnce || m == SubscribePoll {
					v.addPathsFromField(ff, m)
					return nil
				}
				for _, s := range ff.Fields {
					ss, ok := s.(*protobufFieldSequence)
					if !ok || ss.Name != "subscription" {
						continue
					}
					for _, i := range ss.Fields {
						ii, ok := i.(*protobufFieldSequence)
						if !ok {
							continue
						}
						var path *protobufFieldGNMIPath
						var mode *protobufFieldEnum
						for _, j := range ii.Fields {
							switch jj := j.(type) {
							case *protobufFieldGNMIPath:
								path = jj
							case *protobufFieldEnum:
								if jj.Name == "mode" {
									mode = jj
								}
							}
						}
						if mode == nil || path == nil {
							return fmt.Errorf("missing mode or path field")
						}
						m := v.getFieldMode(mode)
						v.addPathsFromField(path, m)
					}
				}
			}
			// There is only one subscribe field.
			break
		}
	}
	return nil
}

// Accept goes through all scenarios and sub-scenarios and calls visitor for each of them.
func (v *coveredPathsVisitor) Accept(d *DOM) {
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
func (v *coveredPathsVisitor) VisitScenario(i *Instruction) interface{} {
	v.scenario = i
	result := i.VisitChildren(v)
	if err, ok := result.(error); ok {
		return fmt.Errorf("scenario: %s: %v", i.Name, err)
	}
	return result
}

// VisitSubScenario is called once for each sub-scenario.
func (v *coveredPathsVisitor) VisitSubScenario(i *Instruction) interface{} {
	v.scenario = i
	result := i.VisitChildren(v)
	if err, ok := result.(error); ok {
		return fmt.Errorf("subScenario: %s: %v", i.Name, err)
	}
	return result
}

// VisitCall is called for each gNMI call instruction.
func (v *coveredPathsVisitor) VisitCall(i *Instruction) interface{} {
	if i.Protobuf != nil {
		if err := v.addPaths(i.Protobuf); err != nil {
			return fmt.Errorf("instruction ID: %d: %v", i.ID, err)
		}
	}
	if i.Response != nil {
		if err := v.addPaths(i.Response); err != nil {
			return fmt.Errorf("instruction ID: %d: %v", i.ID, err)
		}
	}
	return i.VisitChildren(v)
}

// VisitOpenStr is called for each open-stream instruction.
func (v *coveredPathsVisitor) VisitOpenStr(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitCloseStr is called for each close-stream instruction.
func (v *coveredPathsVisitor) VisitCloseStr(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitSend is called for each send instruction.
func (v *coveredPathsVisitor) VisitSend(i *Instruction) interface{} {
	if i.Protobuf != nil {
		if err := v.addPaths(i.Protobuf); err != nil {
			return fmt.Errorf("instruction ID: %d: %v", i.ID, err)
		}
	}
	return i.VisitChildren(v)
}

// VisitReceive is called for each receive instruction.
func (v *coveredPathsVisitor) VisitReceive(i *Instruction) interface{} {
	if i.Protobuf != nil {
		if err := v.addPaths(i.Protobuf); err != nil {
			return fmt.Errorf("instruction ID: %d: %v", i.ID, err)
		}
	}
	return i.VisitChildren(v)
}

// VisitGroupAtLeastOnce is called for each at-least-once instruction group.
func (v *coveredPathsVisitor) VisitGroupAtLeastOnce(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupZeroOrMore is called for each GroupZeroOrMore instruction group.
func (v *coveredPathsVisitor) VisitGroupZeroOrMore(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitGroupAnyOrder is called for each AnyOrder group.
func (v *coveredPathsVisitor) VisitGroupAnyOrder(i *Instruction) interface{} {
	return i.VisitChildren(v)
}

// VisitVarDeclaration is called for each varDeclaration instruction.
func (v *coveredPathsVisitor) VisitVarDeclaration(i *Instruction) interface{} {
	return i.VisitChildren(v)
}
