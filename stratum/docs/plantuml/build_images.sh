#! /bin/bash -
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Set this to your plantuml.jar file
export PLANTUML=~/plantuml.jar

# Script to build the PNG images from the plantuml source

for f in `ls *.puml`
do
  java -jar $PLANTUML $f
done
mv *.png ../images

