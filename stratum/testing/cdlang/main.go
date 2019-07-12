// The cdl is a Contract Definition Language (go/cdlang) transpiler.
// It takes a source file written in CDLang and a Go template file and produces
// an output file that is a result of processing the template file with data
// from the CDLang file.
// Usage:
//   cdl -t template-file.tmpl -o output-file.cc cdlang-input-file.cdlang
package main

import (
	"bytes"
	"errors"
	"io/ioutil"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"text/template"

	"flag"
	"log"
	"cdl"
	"cdlang"
)

type TmplFileNames []string

func (arr *TmplFileNames) String() string {
	ret := ""
	for _, s := range *arr {
		ret = ret + s
	}
	return ret
}

func (arr *TmplFileNames) Set(value string) error {
	*arr = append(*arr, strings.Split(value, ",")...)
	return nil
}

var (
	outputFileName = flag.String("o", "out.txt", "the output file path")
	tmplFileNames  TmplFileNames
	version        = flag.String("v", "latest", "version of scenarios to be used")
	logLevel       = flag.Int("l", 0, "log level")
)

// last() is called from the template.
// It returns `true` if `x` is the index of the last element of the `a` slice.
func last(x int, a interface{}) bool {
	return x == reflect.ValueOf(a).Len()-1
}

// vars() is called from the template.
// It returns a map build from its input parameters. There have to be even number
// of parameters and first parameter of each pair has to be a string that will be
// used as the key and the second parameter will be used as the value.
func vars(values ...interface{}) (map[string]interface{}, error) {
	if len(values)%2 != 0 {
		return nil, errors.New("wrong number of params in vars call")
	}
	dict := make(map[string]interface{}, len(values)/2)
	for i := 0; i < len(values); i += 2 {
		key, ok := values[i].(string)
		if !ok {
			return nil, errors.New("vars keys must be strings")
		}
		dict[key] = values[i+1]
	}
	return dict, nil
}

// arr() is called from the template.
// It returns a slice of object that is build from the function input parameters.
func arr(values ...interface{}) ([]interface{}, error) {
	var arr []interface{}
	for i := 0; i < len(values); i++ {
		switch values[i].(type) {
		case []interface{}:
			arr = append(arr, values[i].([]interface{})...)
		case interface{}:
			arr = append(arr, values[i])
		}
	}
	return arr, nil
}

// concat() is called from the template.
// It returns a string that is build by concatenating all input parameters.
func concat(values ...interface{}) (string, error) {
	result := ""
	for _, s := range values {
		switch s.(type) {
		case string:
			result += s.(string)
		case int:
			result += strconv.FormatInt(int64(s.(int)), 16)
		}
	}
	return result, nil
}

// buildAbstractSyntaxTree() handles processing of CDLang input file.
// If the file is syntaxtically correct, it returns AST.
func buildAbstractSyntaxTreeFromFile(fileName string) cdlang.IContractContext {
	// Read the file contents.
	data, err := ioutil.ReadFile(fileName)
	if err != nil {
		log.Fatalf("cdl: %v", err)
	}
	tree, err := cdl.BuildAbstractSyntaxTree(string(data))
	if err != nil {
		log.Fatalf("cdl: %v", err)
	}
	return tree
}

// processTemplate() executes the template using data from the `dom` object.
func processTemplate(dom *cdl.DOM) {
	if len(tmplFileNames) == 0 {
		log.Fatal("cdl: templates have not been specified.")
	}
	// Create template t by parsing files passed with -t option.
	// The template lib API requires that the name used in New() is base
	// name of one of the files listed in ParseFiles().
	t, err := template.New(filepath.Base(tmplFileNames[0])).Funcs(template.FuncMap{
		"last":   last,
		"vars":   vars,
		"arr":    arr,
		"concat": concat,
		"TeX":    cdl.TeX,
	}).ParseFiles(tmplFileNames...)
	if err != nil {
		log.Fatalf("cdl: %v", err)
	}
	// Produce output file using the template and the DOM.
	// First create a buffer where the output of the template will be stored
	// (it's done to avoid creating incomplete output file).
	var b bytes.Buffer
	// Then, execute the template.
	if err := t.Execute(&b, dom); err != nil {
		log.Fatalf("cdl: %v", err)
	}
	// If everything is OK, then write the output to the output file.
	if err = ioutil.WriteFile(*outputFileName, b.Bytes(), os.ModePerm); err != nil {
		log.Fatalf("cdl: %v", err)
	}
}

func main() {
	flag.Var(&tmplFileNames, "t", "list of templates")
	flag.Parse()
	ver, err := cdl.NewVersion(*version)
	if err != nil {
		log.Fatalf("cdl: %v", err)
	}
	dom := cdl.NewDOM()
	// Process all CDLang input files.
	for _, fileName := range flag.Args() {
		tree := buildAbstractSyntaxTreeFromFile(fileName)
		// Now DOM can be updated by the visitor.
		status := tree.Accept(cdl.NewVisitor(dom))
		if status != nil && status.(error) != nil {
			log.Fatalf("cdl: %s: %v", fileName, status)
		}
	}
	// All input files have been processed. Now the DOM has to be post-processed.
	dom.PostProcess(ver, *logLevel)
	// Dump the DOM the console.
	if *logLevel == 1 {
		log.Println(dom.Marshal())
	}
	// Process the template file with DOM as input.
	processTemplate(dom)
}
