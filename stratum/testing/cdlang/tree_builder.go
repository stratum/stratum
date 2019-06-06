package cdl

// This file implements function that takes string with CDLang program and
// produces an abstract syntax tree.

import (
	"fmt"

	"cdlang"
	"github.com/antlr/antlr4/runtime/Go/antlr"
)

// errorListener is an ANTLR error listener that saves a reference to the first
// syntax error.
type errorListener struct {
	*antlr.DefaultErrorListener
	err error
}

// Customized implementation of antlr.DefaultErrorListener.SyntaxError().
// It stores first error found.
func (l *errorListener) SyntaxError(_ antlr.Recognizer, _ interface{}, line, column int, msg string, _ antlr.RecognitionException) {
	if l.err == nil {
		l.err = fmt.Errorf("syntax error at %d:%d: %s", line, column, msg)
	}
}

// BuildAbstractSyntaxTree handles processing of CDLang program.
// If the file is syntaxtically correct, it returns AST.
func BuildAbstractSyntaxTree(program string) (cdlang.IContractContext, error) {
	input := antlr.NewInputStream(program)
	lexer := cdlang.NewCDLangLexer(input)
	stream := antlr.NewCommonTokenStream(lexer, 0)
	parser := cdlang.NewCDLangParser(stream)
	parser.BuildParseTrees = true

	// Add custom error reporter that reports only the first error.
	e := &errorListener{}
	lexer.RemoveErrorListeners()
	lexer.AddErrorListener(e)
	parser.RemoveErrorListeners()
	parser.AddErrorListener(e)

	// Parse the CDLang file and create AST `tree`.
	tree := parser.Contract()
	if e.err != nil {
		return nil, e.err
	}
	return tree, nil
}
