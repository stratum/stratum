// Copyright 2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// define a grammar called Contract Definition Language
grammar CDLang;
contract:
        (constProto | mapping | scenario | subScenario)*
        ;

constName:
        n=ID
        ;

constProto:
        n=constName ':=' p=protobuf
        ;

domainName:
        ('::')? (ID '::')* ID
        ;

mapping:
        'rpc' fd=domainName '.' f=ID '(' (reqd=domainName '.')? req=ID ')'
            '=>' (respd=domainName '.')? resp=ID
        ;

scenario:
        (disabled='disabled')? sos='scenario' name=ID '(' ')' ver=version '{'
        instruction*
        eos='}'
        ;

subScenario:
        sos='scenario' name=ID '(' variableDeclaration (',' variableDeclaration)* ')' ver=version '{'
        instruction*
        eos='}'
        ;

variableDeclaration:
        name=ID ':' type_name=ID
        ;

variable:
        '$' str_name=ID | skiped='_' | '#' num_name=ID
        ;

constant:
        str=STRING
        ;

version:
        'version' major=NUMBER '.' minor=NUMBER '.' patch=NUMBER
        ;

instruction:
        send | receive | group | execute | openStream | closeStream | call
        ;

call:
        req=protobuf '>>' '{' callResponse '}'
        ;

callResponse:
        (ok='OK' ',' resp=protobuf) | (err='ERROR')
        ;

openStream:
        stream=ID ':=' domain=ID '.' method=ID
        ;

closeStream:
        'close' stream=ID
        ;

send:
        ch=ID '<<' protobuf
        ;

receive:
        ch=ID '>>' protobuf
        ;

group:
        at_least_once='AtLeastOnce' '{' instruction* '}'
        | zero_or_more='ZeroOrMore' '{' instruction* '}'
        | any_order='AnyOrder' '{' receive* '}'
        ;

execute:
        'execute' name=ID '(' variable (',' variable)* ')'
        ;

protobuf:
        (domain=ID '.') ? name=ID '{' protobufField* '}'
        ;

protobufField:
        protobufFieldSimple | protobufFieldGroup | protobufFieldRepeated
        ;

protobufFieldSimple:
        name=ID ':'
        (val_number=NUMBER | val_string=STRING | val_enum=enumer
        | val_var=variable | val_path=path
        )
        ;

enumer:
        ('::')? ID ('::' ID)*
        ;

protobufFieldGroup:
        name=ID ('<' cast=domainName '>')? '{' protobufField* '}'
        ;

protobufFieldRepeated:
        name=ID '[' protobufFieldRepeatedRow+ ']'
        ;

protobufFieldRepeatedRow:
        '{' protobufField+ '}' (zero_or_more='*' | one_or_more='+' | zero_or_one='?')? ','?
        ;

path:
        ('/' pathElement)*
        ;

pathElement:
        name=ID
        | name=ID '[' key=ID '=' (a='*'|rd_var=variable|e=ID) ']'
        | name=ID '[' wr_var=variable ':=' key=ID ']'
        | param=variable
        ;

ID: [a-zA-Z_][a-zA-Z0-9_-]* ;
WS: [ \t\r\n]+ -> skip ;
LINE_COMMENT: '//' ~( '\r' | '\n')* -> skip ;
BLOCK_COMMENT: '/*' .*? '*/' -> skip ;
STRING: '"' .+? '"' ;
NUMBER: '-'? INT ;
fragment INT: '0' | [1-9] [0-9]* ;
