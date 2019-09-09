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

import (
	"strings"
	"testing"
)

func TestParseCDLangCode(t *testing.T) {
	tests := []struct {
		name         string
		inCDLangCode string
		wantDOM      *DOM
	}{
		{
			name:         "empty scenario",
			inCDLangCode: "scenario test() version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test() version 1.2.3 {}",
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with 2 versions: 1.2.3 and 2.3.4",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {}" +
				"scenario test() version 2.3.4 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test() version 1.2.3 {}",
					},
				},
				OtherScenarios: map[string]map[string]*Instruction{
					"test": map[string]*Instruction{
						"2.3.4": {Type: ScenarioInst,
							Name: "test",
							Version: &Version{
								Major: 2,
								Minor: 3,
								Patch: 4,
							},
							Source: "scenario test() version 2.3.4 {}",
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with 2 versions: 1.2.3 and 1.1.0",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {}" +
				"scenario test() version 1.1.0 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test() version 1.2.3 {}",
					},
				},
				OtherScenarios: map[string]map[string]*Instruction{
					"test": map[string]*Instruction{
						"1.1.0": {Type: ScenarioInst,
							Name: "test",
							Version: &Version{
								Major: 1,
								Minor: 1,
								Patch: 0,
							},
							Source: "scenario test() version 1.1.0 {}",
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with 2 versions: 1.2.4 and 1.2.0",
			inCDLangCode: "" +
				"scenario test() version 1.2.4 {}" +
				"scenario test() version 1.2.0 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 0,
						},
						Source: "scenario test() version 1.2.0 {}",
					},
				},
				OtherScenarios: map[string]map[string]*Instruction{
					"test": map[string]*Instruction{
						"1.2.4": {Type: ScenarioInst,
							Name: "test",
							Version: &Version{
								Major: 1,
								Minor: 2,
								Patch: 4,
							},
							Source: "scenario test() version 1.2.4 {}",
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with version 1.2.4",
			inCDLangCode: "" +
				"scenario test() version 1.2.4 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{},
				OtherScenarios: map[string]map[string]*Instruction{
					"test": map[string]*Instruction{
						"1.2.4": {Type: ScenarioInst,
							Name: "test",
							Version: &Version{
								Major: 1,
								Minor: 2,
								Patch: 4,
							},
							Source: "scenario test() version 1.2.4 {}",
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with mapping",
			inCDLangCode: "" +
				"rpc foo.Bar(BarRequest) => BarResponse" +
				" " +
				"scenario test() version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test() version 1.2.3 {}",
					},
				},
				OtherScenarios: map[string]map[string]*Instruction{},
				SubScenarios:   map[string]*Instruction{},
			},
		},
		{
			name: "empty scenario with constant",
			inCDLangCode: "" +
				"kProto := Proto {}" +
				" " +
				"scenario test() version 1.2.3 {}",
			wantDOM: &DOM{
				GlobalInst: map[string]*Instruction{
					"kProto": {Type: ConstProtoInst,
						Name: "ConstProto",
						ID:   0,
					},
				},
				Scenarios: map[string]*Instruction{
					"test": {Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test() version 1.2.3 {}",
					},
				},
				OtherScenarios: map[string]map[string]*Instruction{},
				SubScenarios:   map[string]*Instruction{},
			},
		},
		{

			name:         "disabled empty scenario",
			inCDLangCode: "disabled scenario test() version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type:     ScenarioInst,
						Name:     "test",
						Disabled: true,
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "disabled scenario test() version 1.2.3 {}",
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "send once scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi << SubscribeRequest {" +
				"         subscribe {" +
				"	     mode: ONCE" +
				"	     subscription [ { path: /interface[name=*] } ] } }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi << SubscribeRequest {" +
							"         subscribe {" +
							"	     mode: ONCE" +
							"	     subscription [ { path: /interface[name=*] } ] } }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: SendInst,
								Name: "Send",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "subscribe",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldEnum{
														genericProtobufField: &genericProtobufField{
															Name:      "mode",
															FieldType: enumField,
														},
														Value: "ONCE",
													},
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "subscription",
															FieldType: repeatedField,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeRequest",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/interface[name=\"*\"]": {
						Path: "/interface[name=\"*\"]",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "interface",
										Key: map[string]*Param{
											"name": {
												ParameterKind: stringConstant,
												Name:          "*",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							SubscribeOnce: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "send poll scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi << SubscribeRequest {" +
				"         subscribe {" +
				"	     mode: POLL" +
				"	     subscription [ { path: /interface[name=*] } ] } }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi << SubscribeRequest {" +
							"         subscribe {" +
							"	     mode: POLL" +
							"	     subscription [ { path: /interface[name=*] } ] } }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: SendInst,
								Name: "Send",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "subscribe",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldEnum{
														genericProtobufField: &genericProtobufField{
															Name:      "mode",
															FieldType: enumField,
														},
														Value: "POLL",
													},
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "subscription",
															FieldType: repeatedField,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeRequest",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/interface[name=\"*\"]": {
						Path: "/interface[name=\"*\"]",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "interface",
										Key: map[string]*Param{
											"name": {
												ParameterKind: stringConstant,
												Name:          "*",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							SubscribePoll: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "send subscribe:stream:sample scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi << SubscribeRequest {" +
				"         subscribe {" +
				"	     mode: STREAM" +
				"	     subscription [ { path: /interface[name=*] mode: SAMPLE }" +
				"                     { path: /interface[name=*] mode: ON_CHANGE }" +
				"                     { path: /interface[name=*] mode: ON_CHANGE }" +
				"                     { path: /interface[name=*] mode: TARGET_DEFINED } ] } }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi << SubscribeRequest {" +
							"         subscribe {" +
							"	     mode: STREAM" +
							"	     subscription [ { path: /interface[name=*] mode: SAMPLE }" +
							"                     { path: /interface[name=*] mode: ON_CHANGE }" +
							"                     { path: /interface[name=*] mode: ON_CHANGE }" +
							"                     { path: /interface[name=*] mode: TARGET_DEFINED } ] } }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: SendInst,
								Name: "Send",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "subscribe",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldEnum{
														genericProtobufField: &genericProtobufField{
															Name:      "mode",
															FieldType: enumField,
														},
														Value: "STREAM",
													},
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "subscription",
															FieldType: repeatedField,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																	&protobufFieldEnum{
																		genericProtobufField: &genericProtobufField{
																			Name:      "mode",
																			FieldType: enumField,
																		},
																		Value: "SAMPLE",
																	},
																},
															},
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																	&protobufFieldEnum{
																		genericProtobufField: &genericProtobufField{
																			Name:      "mode",
																			FieldType: enumField,
																		},
																		Value: "ON_CHANGE",
																	},
																},
															},
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																	&protobufFieldEnum{
																		genericProtobufField: &genericProtobufField{
																			Name:      "mode",
																			FieldType: enumField,
																		},
																		Value: "ON_CHANGE",
																	},
																},
															},
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: stringConstant,
																							Name:          "*",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																	&protobufFieldEnum{
																		genericProtobufField: &genericProtobufField{
																			Name:      "mode",
																			FieldType: enumField,
																		},
																		Value: "TARGET_DEFINED",
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeRequest",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/interface[name=\"*\"]": {
						Path: "/interface[name=\"*\"]",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "interface",
										Key: map[string]*Param{
											"name": {
												ParameterKind: stringConstant,
												Name:          "*",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							SubscribeSample:        {"test": true},
							SubscribeOnChange:      {"test": true},
							SubscribeTargetDefined: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with read variable",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"         update {" +
				"	     update [ { path: /interface[name=$ifname] val { uint_val: _ } } ] } }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"         update {" +
							"	     update [ { path: /interface[name=$ifname] val { uint_val: _ } } ] } }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type: VarDeclarationInst,
								Name: "ifname",
								ID:   0,
								Variable: &Param{
									ParameterKind: variable,
									Name:          "ifname",
									ParameterType: "string",
								},
							},
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      1,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   2,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "update",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "update",
															FieldType: repeatedField,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "update",
																	FieldType: sequenceOfFields,
																},
																Fields: []protobufField{
																	&protobufFieldGNMIPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: gnmiPathField,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": {
																							ParameterKind: variable,
																							Name:          "ifname",
																							ParameterType: "string",
																						},
																					},
																				},
																			},
																		},
																	},
																	&protobufFieldSequence{
																		genericProtobufField: &genericProtobufField{
																			Name:      "val",
																			FieldType: sequenceOfFields,
																		},
																		Fields: []protobufField{
																			&protobufFieldVariable{
																				genericProtobufField: &genericProtobufField{
																					Name:      "uint_val",
																					FieldType: variableField,
																				},
																				Parameters: []*Param{
																					{
																						ParameterKind: ignored,
																						Name:          "<ignored>",
																						ParameterType: "<ignored>",
																					},
																				},
																			},
																		},
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      3,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/interface[name=\"$ifname\"]": {
						Path: "/interface[name=\"$ifname\"]",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "interface",
										Key: map[string]*Param{
											"name": {
												ParameterKind: variable,
												Name:          "ifname",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							Response: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with write variable",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     path: /interface[$ifname:=name] }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     path: /interface[$ifname:=name] }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type: VarDeclarationInst,
								Name: "ifname",
								ID:   0,
								Variable: &Param{
									ParameterKind: variableDecl,
									Name:          "ifname",
									ParameterType: "string",
								},
							},
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      1,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   2,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldGNMIPath{
												genericProtobufField: &genericProtobufField{
													Name:      "path",
													FieldType: gnmiPathField,
												},
												Value: &gnmiPath{
													Elem: []*gnmiPathElem{
														{
															Name: "interface",
															Key: map[string]*Param{
																"name": {
																	ParameterKind: variableDecl,
																	Name:          "ifname",
																	ParameterType: "string",
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      3,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/interface[name=\"$ifname\"]": {
						Path: "/interface[name=\"$ifname\"]",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "interface",
										Key: map[string]*Param{
											"name": {
												ParameterKind: variableDecl,
												Name:          "ifname",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							Response: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with one-or-more updates",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     update [ { val: _ }+ ] }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     update [ { val: _ }+ ] }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "update",
													FieldType: repeatedField,
												},
												Fields: []protobufField{
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "update",
															FieldType: sequenceOfFields,
														},
														SequenceSize: protobufFieldSequenceSizeOneOrMore,
														Fields: []protobufField{
															&protobufFieldVariable{
																genericProtobufField: &genericProtobufField{
																	Name:      "val",
																	FieldType: variableField,
																},
																Parameters: []*Param{
																	{
																		ParameterKind: ignored,
																		Name:          "<ignored>",
																		ParameterType: "<ignored>",
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with zero-or-more updates",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     update [ { val: _ }* ] }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     update [ { val: _ }* ] }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "update",
													FieldType: repeatedField,
												},
												Fields: []protobufField{
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "update",
															FieldType: sequenceOfFields,
														},
														SequenceSize: protobufFieldSequenceSizeZeroOrMore,
														Fields: []protobufField{
															&protobufFieldVariable{
																genericProtobufField: &genericProtobufField{
																	Name:      "val",
																	FieldType: variableField,
																},
																Parameters: []*Param{
																	{
																		ParameterKind: ignored,
																		Name:          "<ignored>",
																		ParameterType: "<ignored>",
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with zero-or-one updates",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     update [ { val: _ }? ] }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     update [ { val: _ }? ] }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "update",
													FieldType: repeatedField,
												},
												Fields: []protobufField{
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "update",
															FieldType: sequenceOfFields,
														},
														SequenceSize: protobufFieldSequenceSizeZeroOrOne,
														Fields: []protobufField{
															&protobufFieldVariable{
																genericProtobufField: &genericProtobufField{
																	Name:      "val",
																	FieldType: variableField,
																},
																Parameters: []*Param{
																	{
																		ParameterKind: ignored,
																		Name:          "<ignored>",
																		ParameterType: "<ignored>",
																	},
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with string variable as path element",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     path: /$ifname }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     path: /$ifname }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type: VarDeclarationInst,
								Name: "ifname",
								ID:   0,
								Variable: &Param{
									ParameterKind: variable,
									Name:          "ifname",
									ParameterType: "string",
								},
							},
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      1,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   2,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldGNMIPath{
												genericProtobufField: &genericProtobufField{
													Name:      "path",
													FieldType: gnmiPathField,
												},
												Value: &gnmiPath{
													Elem: []*gnmiPathElem{
														{
															Name: "",
															Parameters: []*Param{
																&Param{
																	ParameterKind: variable,
																	Name:          "ifname",
																	ParameterType: "string",
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      3,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/": {
						Path: "/",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "",
										Parameters: []*Param{
											&Param{
												ParameterKind: variable,
												Name:          "ifname",
												ParameterType: "string",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							Response: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with number variable as path element",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     path: /#ifnumber }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     path: /#ifnumber }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type: VarDeclarationInst,
								Name: "ifnumber",
								ID:   0,
								Variable: &Param{
									ParameterKind: variable,
									Name:          "ifnumber",
									ParameterType: "int64",
								},
							},
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      1,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   2,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldGNMIPath{
												genericProtobufField: &genericProtobufField{
													Name:      "path",
													FieldType: gnmiPathField,
												},
												Value: &gnmiPath{
													Elem: []*gnmiPathElem{
														{
															Name: "",
															Parameters: []*Param{
																&Param{
																	ParameterKind: variable,
																	Name:          "ifnumber",
																	ParameterType: "int64",
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      3,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/": {
						Path: "/",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "",
										Parameters: []*Param{
											&Param{
												ParameterKind: variable,
												Name:          "ifnumber",
												ParameterType: "int64",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							Response: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "receive scenario with ignored path element",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  gnmi >> SubscribeResponse {" +
				"	     path: /_ }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  gnmi >> SubscribeResponse {" +
							"	     path: /_ }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: ReceiveInst,
								Name: "Receive",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
										Fields: []protobufField{
											&protobufFieldGNMIPath{
												genericProtobufField: &genericProtobufField{
													Name:      "path",
													FieldType: gnmiPathField,
												},
												Value: &gnmiPath{
													Elem: []*gnmiPathElem{
														{
															Name: "",
															Parameters: []*Param{
																&Param{
																	ParameterKind: ignored,
																	Name:          "<ignored>",
																	ParameterType: "<ignored>",
																},
															},
														},
													},
												},
											},
										},
									},
									TypeName: "SubscribeResponse",
								},
								Channel: "gnmi",
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      2,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
				CoveredPaths: map[string]*Path{
					"/": {
						Path: "/",
						Protobuf: &protobufFieldGNMIPath{
							genericProtobufField: &genericProtobufField{
								Name:      "path",
								FieldType: gnmiPathField,
							},
							Value: &gnmiPath{
								Elem: []*gnmiPathElem{
									{
										Name: "",
										Parameters: []*Param{
											&Param{
												ParameterKind: ignored,
												Name:          "<ignored>",
												ParameterType: "<ignored>",
											},
										},
									},
								},
							},
						},
						Scenarios: map[GNMIReqMode]map[string]bool{
							Response: {"test": true},
						},
					},
				},
			},
		},
		{
			name: "any-order scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  AnyOrder {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  AnyOrder {" +
							"    gnmi >> SubscribeResponse { id: 1 }" +
							"    gnmi >> SubscribeResponse { id: 2 }" +
							"  }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: GroupAnyOrderInst,
								Name: "AnyOrder",
								ID:   1,
								Children: []*Instruction{
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   3,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 2,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
								},
								ChildrenPerChannel: map[string][]*Instruction{
									"gnmi": []*Instruction{
										{
											Type: ReceiveInst,
											Name: "Receive",
											ID:   2,
											Protobuf: &protobuf{
												protobufFieldSequence: &protobufFieldSequence{
													genericProtobufField: &genericProtobufField{
														Name:      "",
														FieldType: sequenceOfFields,
													},
													Fields: []protobufField{
														&protobufFieldInt{
															genericProtobufField: &genericProtobufField{
																Name:      "id",
																FieldType: numberField,
															},
															Value: 1,
														},
													},
												},
												TypeName: "SubscribeResponse",
											},
											Channel: "gnmi",
										},
										{
											Type: ReceiveInst,
											Name: "Receive",
											ID:   3,
											Protobuf: &protobuf{
												protobufFieldSequence: &protobufFieldSequence{
													genericProtobufField: &genericProtobufField{
														Name:      "",
														FieldType: sequenceOfFields,
													},
													Fields: []protobufField{
														&protobufFieldInt{
															genericProtobufField: &genericProtobufField{
																Name:      "id",
																FieldType: numberField,
															},
															Value: 2,
														},
													},
												},
												TypeName: "SubscribeResponse",
											},
											Channel: "gnmi",
										},
									},
								},
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      4,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "at-least-once scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  AtLeastOnce {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  AtLeastOnce {" +
							"    gnmi >> SubscribeResponse { id: 1 }" +
							"    gnmi >> SubscribeResponse { id: 2 }" +
							"  }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: GroupAtLeastOnceInst,
								Name: "AtLeastOnce",
								ID:   1,
								Children: []*Instruction{
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   3,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 2,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
								},
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      4,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "zero-or-more scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  ZeroOrMore {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  ZeroOrMore {" +
							"    gnmi >> SubscribeResponse { id: 1 }" +
							"    gnmi >> SubscribeResponse { id: 2 }" +
							"  }" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type: GroupZeroOrMoreInst,
								Name: "ZeroOrMore",
								ID:   1,
								Children: []*Instruction{
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									{
										Type: ReceiveInst,
										Name: "Receive",
										ID:   3,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: sequenceOfFields,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: numberField,
														},
														Value: 2,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
								},
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      4,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name:         "empty sub-scenario",
			inCDLangCode: "scenario test(ifindex: string) version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{},
				SubScenarios: map[string]*Instruction{
					"test": {
						Type: SubScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "scenario test(ifindex: string) version 1.2.3 {}",
						Params: []*Param{
							{
								ParameterKind: declaration,
								Name:          "ifindex",
								ParameterType: "string",
							},
						},
					},
				},
			},
		},
		{
			name: "open-gnmi scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      1,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "open-ctrl scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  ctrl := ctrl.Execute" +
				"  close ctrl" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  ctrl := ctrl.Execute" +
							"  close ctrl" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenCTRLStrInst,
								Name:    "OpenCTRLStream",
								Channel: "ctrl",
								ID:      0,
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "ctrl",
								ID:      1,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "close-gnmi scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi := gNMI.Subscribe" +
				"  close gnmi" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  gnmi := gNMI.Subscribe" +
							"  close gnmi" +
							"}",
						Children: []*Instruction{
							{
								Type:    OpenGNMIStrInst,
								Name:    "OpenGNMIStream",
								Channel: "gnmi",
								ID:      0,
							},
							{
								Type:    CloseStrInst,
								Name:    "CloseStream",
								Channel: "gnmi",
								ID:      1,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "gnmi-get-ok scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  GetRequest {} >> { OK, GetResponse {} }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  GetRequest {} >> { OK, GetResponse {} }" +
							"}",
						Children: []*Instruction{
							{
								Type: CallInst,
								Name: "Call",
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "GetRequest",
								},
								Response: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "GetResponse",
								},
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "gnmi-get-error scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  GetRequest {} >> { ERROR }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  GetRequest {} >> { ERROR }" +
							"}",
						Children: []*Instruction{
							{
								Type: CallInst,
								Name: "Call",
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "GetRequest",
								},
								ErrorExpected: true,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "gnmi-set-ok scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  SetRequest {} >> { OK, SetResponse {} }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  SetRequest {} >> { OK, SetResponse {} }" +
							"}",
						Children: []*Instruction{
							{
								Type: CallInst,
								Name: "Call",
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "SetRequest",
								},
								Response: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "SetResponse",
								},
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "gnmi-set-error scenario",
			inCDLangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  SetRequest {} >> { ERROR }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": {
						Type: ScenarioInst,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Source: "" +
							"scenario test() version 1.2.3 {" +
							"  SetRequest {} >> { ERROR }" +
							"}",
						Children: []*Instruction{
							{
								Type: CallInst,
								Name: "Call",
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: sequenceOfFields,
										},
									},
									TypeName: "SetRequest",
								},
								ErrorExpected: true,
							},
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
	}

	for _, tt := range tests {
		tree, err := BuildAbstractSyntaxTree(tt.inCDLangCode)
		if err != nil {
			t.Errorf("%s: unexpected error during parsing: %v", tt.name, err)
			continue
		}
		gotDOM := NewDOM()
		status := tree.Accept(NewVisitor(gotDOM))
		if status != nil && status.(error) != nil {
			t.Errorf("%s: inCDLangCode(%v): got unexpected error: %v", tt.name, tt.inCDLangCode, status)
			continue
		}
		gotDOM.PostProcess(&Version{Major: 1, Minor: 2, Patch: 3}, 0)
		want, err := tt.wantDOM.Marshal()
		if err != nil {
			t.Errorf("%s: wantDOM(%v): got unexpected error: %v", tt.name, tt.wantDOM, err)
			continue
		}
		got, err := gotDOM.Marshal()
		if err != nil {
			t.Errorf("%s: gotDOM(%v): got unexpected error: %v", tt.name, gotDOM, err)
			continue
		}
		if diff := strings.Compare(want, got); diff != 0 {
			t.Errorf("%s: inCDLangCode(%v): got invalid DOM, diff(-want,+got):\n%s", tt.name, tt.inCDLangCode, diff)
			continue
		}
	}
}

func TestErrorWhileParseCDLangCode(t *testing.T) {
	tests := []struct {
		name         string
		inCDLangCode string
	}{
		{
			name:         "wrong output stream name",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNMI.Subscribe gNMI << Proto{} }",
		},
		{
			name:         "wrong domain name",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNNN.Subscribe gNMI << Proto{} }",
		},
		{
			name:         "wrong method name",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNMI.Execute gNMI << Proto{} }",
		},
	}
	for _, tt := range tests {
		tree, err := BuildAbstractSyntaxTree(tt.inCDLangCode)
		if err != nil {
			t.Errorf("%s: unexpected error during parsing: %v", tt.name, err)
			continue
		}
		gotDOM := NewDOM()
		status := tree.Accept(NewVisitor(gotDOM))
		if status == nil || status.(error) == nil {
			t.Errorf("%s: The syntax error report is nil.", tt.name)
			continue
		}
	}
}

func TestErrorWhileBuildDOMFromCDLangCode(t *testing.T) {
	tests := []struct {
		name         string
		inCDLangCode string
	}{
		{
			name:         "duplicate scenario name",
			inCDLangCode: "scenario test1() version 1.2.3 { } scenario test1() version 1.2.3 { }",
		},
		{
			name:         "duplicate sub-scenario name",
			inCDLangCode: "scenario test1(i: int) version 1.2.3 { } scenario test1(i: int) version 1.2.3 { }",
		},
		{
			name:         "duplicate stream open",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNMI.Subscribe gnmi := gNMI.Subscribe }",
		},
		{
			name:         "duplicate stream close",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNMI.Subscribe close gnmi close gnmi }",
		},
		{
			name:         "stream not open (send)",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi << Proto{} }",
		},
		{
			name:         "stream not open (receive)",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi >> Proto{} }",
		},
		{
			name:         "missing stream close in scenario",
			inCDLangCode: "scenario test() version 1.2.3 { gnmi := gNMI.Subscribe }",
		},
		{
			name:         "missing stream close in a sub-scenario",
			inCDLangCode: "scenario test(i: int) version 1.2.3 { gnmi := gNMI.Subscribe }",
		},
		{
			name: "RPC re-definition different namespaces",
			inCDLangCode: "rpc foo.Bar(BarRequest) => BarResponse" +
				" " +
				"rpc bar.Bar(BarRequest) => BarResponse",
		},
		{
			name: "RPC re-definition different function names",
			inCDLangCode: "rpc foo.Bar1(BarRequest) => BarResponse" +
				" " +
				"rpc foo.Bar2(BarRequest) => BarResponse",
		},
		{
			name: "const re-definition",
			inCDLangCode: "kConst := Proto {}" +
				" " +
				"kConst := Proto {}",
		},
	}
	for _, tt := range tests {
		tree, err := BuildAbstractSyntaxTree(tt.inCDLangCode)
		if err != nil {
			t.Errorf("%s: inCDLangCode(%v): %v", tt.name, tt.inCDLangCode, err)
			continue
		}
		gotDOM := NewDOM()
		status := tree.Accept(NewVisitor(gotDOM))
		if status == nil || status.(error) == nil {
			t.Errorf("%s: inCDLangCode(%v): the visitor error report is nil", tt.name, tt.inCDLangCode)
		}
	}
}

func TestTeX(t *testing.T) {
	tests := []struct {
		name       string
		inString   string
		wantString string
	}{
		{
			"underscore",
			"a_b",
			"a\\_b",
		},
		{
			"dollar",
			"a$b",
			"ab",
		},
	}

	for _, tt := range tests {
		got := TeX(tt.inString)
		if diff := strings.Compare(tt.wantString, got); diff != 0 {
			t.Errorf("%s: inString%v): got invalid string, diff(-want,+got):\n%s", tt.name, tt.inString, diff)
			continue
		}
	}
}

func TestGNMIReqModeToString(t *testing.T) {
	tests := []struct {
		name       string
		inMode     GNMIReqMode
		wantString string
	}{
		{
			"get",
			Get,
			"GET",
		},
		{
			"set",
			Set,
			"SET",
		},
		{
			"subscribe:once",
			SubscribeOnce,
			"SUBSCRIBE:ONCE",
		},
	}

	for _, tt := range tests {
		got := tt.inMode.String()
		if diff := strings.Compare(tt.wantString, got); diff != 0 {
			t.Errorf("%s: inMode%v): got invalid string, diff(-want,+got):\n%s", tt.name, tt.inMode, diff)
			continue
		}
	}
}

func TestCallInputProtoToFuncName(t *testing.T) {
	tests := []struct {
		name          string
		inInstruction *Instruction
		wantFuncName  string
		wantNamespace string
	}{
		{
			"gNMI::GetRequest -> gNMI::Get",
			&Instruction{
				Type: CallInst,
				Name: "Call",
				Protobuf: &protobuf{
					protobufFieldSequence: &protobufFieldSequence{
						genericProtobufField: &genericProtobufField{
							Name:      "",
							FieldType: sequenceOfFields,
						},
					},
					TypeName: "GetRequest",
				},
				Response: &protobuf{
					protobufFieldSequence: &protobufFieldSequence{
						genericProtobufField: &genericProtobufField{
							Name:      "",
							FieldType: sequenceOfFields,
						},
					},
					TypeName: "GetResponse",
				},
			},
			"Get",
			"gnmi",
		},
		{
			"ctrl::ExecuteRequest -> ctrl::Execute",
			&Instruction{
				Type: SendInst,
				Name: "Send",
				ID:   2,
				Protobuf: &protobuf{
					protobufFieldSequence: &protobufFieldSequence{
						genericProtobufField: &genericProtobufField{
							Name:      "",
							FieldType: sequenceOfFields,
						},
						Fields: []protobufField{
							&protobufFieldInt{
								genericProtobufField: &genericProtobufField{
									Name:      "id",
									FieldType: numberField,
								},
								Value: 1,
							},
						},
					},
					TypeName: "ExecuteRequest",
				},
				Channel: "ctrl",
			},
			"Execute",
			"ctrl",
		},
		{
			"unknown type: BlahRequest -> ?",
			&Instruction{
				Type: SendInst,
				Name: "Send",
				ID:   2,
				Protobuf: &protobuf{
					protobufFieldSequence: &protobufFieldSequence{
						genericProtobufField: &genericProtobufField{
							Name:      "",
							FieldType: sequenceOfFields,
						},
						Fields: []protobufField{
							&protobufFieldInt{
								genericProtobufField: &genericProtobufField{
									Name:      "id",
									FieldType: numberField,
								},
								Value: 1,
							},
						},
					},
					TypeName: "BlahRequest",
				},
				Channel: "blah",
			},
			"?",
			"?",
		},
	}
	for _, tt := range tests {
		got := tt.inInstruction.FuncName()
		if diff := strings.Compare(tt.wantFuncName, got); diff != 0 {
			t.Errorf("%s: inInstruction %+v): got invalid function name, diff(-want,+got):\n%s", tt.name, tt.inInstruction, diff)
			continue
		}
		got = tt.inInstruction.Protobuf.Namespace()
		if diff := strings.Compare(tt.wantNamespace, got); diff != 0 {
			t.Errorf("%s: inInstruction %+v): got invalid protobuf namespace, diff(-want,+got):\n%s", tt.name, tt.inInstruction, diff)
			continue
		}
	}

}

func TestParamIsType(t *testing.T) {
	tests := []struct {
		name        string
		inParamType ParameterKind
		inString    string
		want        bool
	}{
		{
			"variable on variable",
			variable,
			"variable",
			true,
		},
		{
			"ignored on variable",
			variable,
			"ignored",
			false,
		},
	}

	for _, tt := range tests {
		got := tt.inParamType.IsType(tt.inString)
		if got != tt.want {
			t.Errorf("%s: inParamType %v): got wrong result, diff(-want,+got): -%v, +%v\n", tt.name, tt.inParamType, tt.want, got)
			continue
		}
	}
}

func TestProtobufFieldIsType(t *testing.T) {
	tests := []struct {
		name     string
		inField  *genericProtobufField
		inString string
		want     bool
	}{
		{
			"variable on variable",
			newGenericProtobufField(variableField),
			"variable",
			true,
		},
		{
			"enum on variable",
			newGenericProtobufField(variableField),
			"enum",
			false,
		},
		{
			"correct-name on incorrect-type",
			newGenericProtobufField(100),
			"enum",
			false,
		},
	}

	for _, tt := range tests {
		got := tt.inField.IsType(tt.inString)
		if got != tt.want {
			t.Errorf("%s: inField %v): got wrong result, diff(-want,+got): -%v, +%v\n", tt.name, tt.inField, tt.want, got)
			continue
		}
	}
}

func TestGNMIPathElemIsType(t *testing.T) {
	tests := []struct {
		name     string
		inElem   *gnmiPathElem
		inString string
		want     bool
	}{
		{
			"ignored on ignored",
			&gnmiPathElem{
				Key: map[string]*Param{},
				Parameters: []*Param{
					newIgnoredValue(),
				},
			},
			"ignored",
			true,
		},
		{
			"variable on ignored",
			&gnmiPathElem{
				Key: map[string]*Param{},
				Parameters: []*Param{
					newIgnoredValue(),
				},
			},
			"variable",
			false,
		},
		{
			"ignored on non-existing",
			&gnmiPathElem{
				Key: map[string]*Param{},
			},
			"ignored",
			false,
		},
	}

	for _, tt := range tests {
		got := tt.inElem.IsType(tt.inString)
		if got != tt.want {
			t.Errorf("%s: inElem %v): got wrong result, diff(-want,+got): -%v, +%v\n", tt.name, tt.inElem, tt.want, got)
			continue
		}
	}
}

func TestGNMIPathElemIsIgnored(t *testing.T) {
	tests := []struct {
		name   string
		inElem *gnmiPathElem
		want   bool
	}{
		{
			"ignored on ignored",
			&gnmiPathElem{
				Key: map[string]*Param{},
				Parameters: []*Param{
					newIgnoredValue(),
				},
			},
			true,
		},
		{
			"not-ignored on variable",
			&gnmiPathElem{
				Key: map[string]*Param{},
				Parameters: []*Param{
					newVariable("name", "type"),
				},
			},
			false,
		},
		{
			"not-ignored on non-existing",
			&gnmiPathElem{
				Key: map[string]*Param{},
			},
			false,
		},
	}

	for _, tt := range tests {
		got := tt.inElem.Ignore()
		if got != tt.want {
			t.Errorf("%s: inElem %v): got wrong result, diff(-want,+got): -%v, +%v\n", tt.name, tt.inElem, tt.want, got)
			continue
		}
	}
}

func TestProtobufSequenceIsSize(t *testing.T) {
	tests := []struct {
		name     string
		inSize   protobufFieldSequenceSize
		inString string
		want     bool
	}{
		{
			"one on one",
			protobufFieldSequenceSizeOne,
			"1",
			true,
		},
		{
			"zero-or-more on one",
			protobufFieldSequenceSizeOne,
			"*",
			false,
		},
		{
			"zero-or-more on error",
			100,
			"*",
			false,
		},
		{
			"zero-or-one on zero-or-one",
			protobufFieldSequenceSizeZeroOrOne,
			"?",
			true,
		},
		{
			"zero-or-more on zero-or-more",
			protobufFieldSequenceSizeZeroOrMore,
			"*",
			true,
		},
		{
			"one-or-more on one-or-more",
			protobufFieldSequenceSizeOneOrMore,
			"+",
			true,
		},
	}

	for _, tt := range tests {
		f := newProtobufFieldRepeatedRow()
		f.SequenceSize = tt.inSize
		got := f.IsSize(tt.inString)
		if got != tt.want {
			t.Errorf("%s: inSize %v): got wrong result, diff(-want,+got): -%v, +%v\n", tt.name, tt.inSize, tt.want, got)
			continue
		}
	}
}
