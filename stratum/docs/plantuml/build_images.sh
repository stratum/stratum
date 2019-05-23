#! /bin/bash -

# Set this to your plantuml.jar file
export PLANTUML=~/plantuml.jar

# Script to build the PNG images from the plantuml source

for f in `ls *.puml`
do
  java -jar $PLANTUML $f 
done
mv *.png ../images

