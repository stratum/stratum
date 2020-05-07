<!--
Copyright 2019 Google LLC
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# gNMI/OpenConfig Contract Definition Language tools

## Background

Both gNMI and OpenConfig specifications are huge and list massive number of
features, options and entities. This is understandable as their goal is to cover
all aspects of network devices configuration, but in a particular usage
scenario, like Google’s SDN-managed data center network, only a small part of
those specs is applicable. How small and which elements are actually needed is
not obvious from the analysis of those specifications and they need to be
explicitly listed in order to make sure that all interested parties support the
right set. Moreover, there is a need to make sure that a third-party’s
implementation of the required features and options is complete without
comparing the feature list or manually testing the switch. Additional complexity
is added by the fact that OpenConfig management model is not a simple
‘fire-and-forget’ protocol but heavily depends on the sequence of operations as
well as notifications sent by the switch in response to external events.

## Overview

Due to the sequential nature of the gNMI/OpenConfig protocol the best way to
describe the contract is a set of scenarios that focus on specific usage cases
and explicitly list messages being exchanged interleaved with event triggers
and content checks. Those scenarios then can be used to develop tests that will
check if the device under test is following the expected behavior. To make the
development of the scenario files easier for humans a domain-specific language
called Contract Definition Language (CDLang) is used. Moreover, usage of formal
description language allows for the development of a transpiler (a tool that
generates source code in one language based on an input written in another
language)that will automatically convert the contract definition files into unit
tests that will be compiled and executed as well as to generate documentation.

## Design

The design document can be found
[here](g3doc/cdlang.md)

## Implementation

A domain-specific language can be implemented in a number of ways, which include
(but is not limited to):

-   writing tokenizer and parser manually

-   using library that is part of the `golang`

-   using simple tools like
    [`lex`](https://en.wikipedia.org/wiki/Lex_(software))
    & [`yacc`](https://en.wikipedia.org/wiki/Yacc)

-   using more advanced tools like [`antlr` ](http://www.antlr.org/)

Out of those options, using antlr gives the best return to investment due to its
advanced capabilities that allow for focusing on important parts that need to be
implemented leaving the code that can be generated to be handled by `antlr`.

`antlr` takes as an input a file with a language grammar as an input and uses it
to generate both a lexer and a parser that are used to process input files
written in the language defined by the grammar file. In the case of CDLang, the
file that contains the grammar is: `CDLang.g4`.

Additionally, the `antlr` generates a skeleton of code that is base for
implementation of the `visitor design pattern` that is used to traverse the tree
created by the parser and to extract all important information from it. In the
case of CDLang the visitor is implemented in: `visitor.go`.

This information collected by the visitor is stored in a document object model,
an object that is then passed to the golang template processing engine together
with a template file. The template engine replaces special fields in the
template file with data from the document object model.

There can be multiple template files, like `scenarios.cc.tmpl` located in
//platforms/networking/stratum/testing/gnmi_standalone.

The document object model is implemented by the code in: `dom.go`.

`dom_visitor.go` implements the Visitor Design Pattern to post-process the
initial DOM object. In this process information that was not explicitly written
in the CDLang sources is computed and added. For example, this file implements a
visitor that assigns unique ID to each CDLang instruction.

`main.go`is the file that glues all the parts together.

`main_test.go` contains tests testing the `cdl_tool`. It contains snippets
written in CDLang that are processed by the `cdl_tool` and then compared with
the expected output DOM object.

## Usage

### Build the `cdl`transpiler

To build the transpiler execute:

```shell
blaze build :cdl_tool
```

### Generating the output file

To translate CDLang source files into an output file use the following command:

```shell
blaze run :cdl_tool -- -o output-file.cc -t template-file.tmpl cdlang-source-file.cdl
```
