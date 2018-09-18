package cdl

import (
	"testing"

	"google3/golang/godebug/pretty/pretty"
)

func TestParseCdlangCode(t *testing.T) {
	tests := []struct {
		name         string
		inCdlangCode string
		wantDOM      *DOM
	}{
		{
			name:         "empty scenario",
			inCdlangCode: "scenario test() version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "send scenario",
			inCdlangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi << SubscribeRequest {" +
				"         subscribe {" +
				"	     mode: ONCE" +
				"	     subscription [ { path: /interface[name=*] } ] } }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Children: []*Instruction{
							&Instruction{
								Type: 0,
								Name: "Send",
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: 5,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "subscribe",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldEnum{
														genericProtobufField: &genericProtobufField{
															Name:      "mode",
															FieldType: 3,
														},
														Value: "ONCE",
													},
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "subscription",
															FieldType: 6,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "subscription",
																	FieldType: 5,
																},
																Fields: []protobufField{
																	&protobufFieldGnmiPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: 2,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				&gnmiPathElem{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": &Param{
																							ParameterKind: 1,
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
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "receive scenario with variable",
			inCdlangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  gnmi >> SubscribeResponse {" +
				"         update {" +
				"	     update [ { path: /interface[name=$ifname] val { uint_val: _ } } ] } }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Children: []*Instruction{
							&Instruction{
								Type: 8,
								Name: "ifname",
								ID:   0,
								Variable: &Param{
									ParameterKind: 0,
									Name:          "ifname",
									ParameterType: "string",
								},
							},
							&Instruction{
								Type: 1,
								Name: "Receive",
								ID:   1,
								Protobuf: &protobuf{
									protobufFieldSequence: &protobufFieldSequence{
										genericProtobufField: &genericProtobufField{
											Name:      "",
											FieldType: 5,
										},
										Fields: []protobufField{
											&protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "update",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldSequence{
														genericProtobufField: &genericProtobufField{
															Name:      "update",
															FieldType: 6,
														},
														Fields: []protobufField{
															&protobufFieldSequence{
																genericProtobufField: &genericProtobufField{
																	Name:      "update",
																	FieldType: 5,
																},
																Fields: []protobufField{
																	&protobufFieldGnmiPath{
																		genericProtobufField: &genericProtobufField{
																			Name:      "path",
																			FieldType: 2,
																		},
																		Value: &gnmiPath{
																			Elem: []*gnmiPathElem{
																				&gnmiPathElem{
																					Name: "interface",
																					Key: map[string]*Param{
																						"name": &Param{
																							ParameterKind: 0,
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
																			FieldType: 5,
																		},
																		Fields: []protobufField{
																			&protobufFieldVariable{
																				genericProtobufField: &genericProtobufField{
																					Name:      "uint_val",
																					FieldType: 4,
																				},
																				Parameters: []*Param{
																					&Param{
																						ParameterKind: 4,
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
						},
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "any-order scenario",
			inCdlangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  AnyOrder {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Children: []*Instruction{
							&Instruction{
								Type: 3,
								Name: "AnyOrder",
								ID:   0,
								Children: []*Instruction{
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   1,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
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
								GNMI: []*Instruction{
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   1,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
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
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "at-least-once scenario",
			inCdlangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  AtLeastOnce {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Children: []*Instruction{
							&Instruction{
								Type: 5,
								Name: "AtLeastOnce",
								ID:   0,
								Children: []*Instruction{
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   1,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
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
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name: "zero-or-more scenario",
			inCdlangCode: "" +
				"scenario test() version 1.2.3 {" +
				"  ZeroOrMore {" +
				"    gnmi >> SubscribeResponse { id: 1 }" +
				"    gnmi >> SubscribeResponse { id: 2 }" +
				"  }" +
				"}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 9,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Children: []*Instruction{
							&Instruction{
								Type: 4,
								Name: "ZeroOrMore",
								ID:   0,
								Children: []*Instruction{
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   1,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
														},
														Value: 1,
													},
												},
											},
											TypeName: "SubscribeResponse",
										},
										Channel: "gnmi",
									},
									&Instruction{
										Type: 1,
										Name: "Receive",
										ID:   2,
										Protobuf: &protobuf{
											protobufFieldSequence: &protobufFieldSequence{
												genericProtobufField: &genericProtobufField{
													Name:      "",
													FieldType: 5,
												},
												Fields: []protobufField{
													&protobufFieldInt{
														genericProtobufField: &genericProtobufField{
															Name:      "id",
															FieldType: 1,
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
					},
				},
				SubScenarios: map[string]*Instruction{},
			},
		},
		{
			name:         "empty sub-scenario",
			inCdlangCode: "scenario test(ifindex: string) version 1.2.3 {}",
			wantDOM: &DOM{
				Scenarios: map[string]*Instruction{},
				SubScenarios: map[string]*Instruction{
					"test": &Instruction{
						Type: 10,
						Name: "test",
						Version: &Version{
							Major: 1,
							Minor: 2,
							Patch: 3,
						},
						Params: []*Param{
							{
								ParameterKind: 3,
								Name:          "ifindex",
								ParameterType: "string",
							},
						},
					},
				},
			},
		}}

	for _, tt := range tests {
		tree, err := BuildAbstractSyntaxTree(tt.inCdlangCode)
		if err != nil {
			t.Errorf("%s: unexpected error during parsing: %v", tt.name, err)
			continue
		}
		gotDOM := NewDOM()
		status := tree.Accept(NewVisitor(gotDOM))
		if status != nil && status.(error) != nil {
			t.Errorf("%s: inCdlangCode(%v): got unexpected error: %v", tt.name, tt.inCdlangCode, status)
			continue
		}
		gotDOM.PostProcess()
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
		if diff := pretty.Compare(got, want); diff != "" {
			t.Errorf("%s: inCdlangCode(%v): got invalid DOM, diff(-got,+want):\n%s", tt.name, tt.inCdlangCode, diff)
			continue
		}
	}
}

func TestErrorWhileParseCdlangCode(t *testing.T) {
	withError := "scenario test() version 1.2.3 { other << Proto{} }"
	if _, err := BuildAbstractSyntaxTree(withError); err == nil {
		t.Error("The syntax error report is nil.")
	}
}
