// Copyright 2022-present Intel
// SPDX-License-Identifier: Apache-2.0
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"strings"
	"text/template"
)

type Variables struct {
	Environment         map[string]string
	Branch              string
	Revision            string
	StratumBuilderImage string
	PublishDockerBuild  bool
}

func main() {
	var rebuildBuilderImage bool
	flag.BoolVar(&rebuildBuilderImage, "new-builder", false, "Rebuild Stratum builder image")
	flag.Parse()
	t, err := template.ParseFiles("generated_config.yml")
	if err != nil {
		log.Fatal(err)
	}

	envMap := make(map[string]string)
	for _, env := range os.Environ() {
		kv := strings.Split(env, "=")
		envMap[kv[0]] = kv[1]
	}
	stratumBuilderImage := "stratumproject/build:build"
	if rebuildBuilderImage {
		stratumBuilderImage = fmt.Sprintf("stratumproject/build:ci-%s", envMap["CIRCLE_BRANCH"])
	}
	data := Variables{
		Environment:         envMap,
		Branch:              envMap["CIRCLE_BRANCH"],
		Revision:            envMap["CIRCLE_SHA1"],
		StratumBuilderImage: stratumBuilderImage,
		PublishDockerBuild:  rebuildBuilderImage,
	}

	err = t.Execute(os.Stdout, data)
	if err != nil {
		log.Fatal(err)
	}
}
