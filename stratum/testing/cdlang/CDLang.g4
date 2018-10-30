// define a grammar called Contract Definition Language
grammar CDLang;
contract:
        (scenario | subScenario)*
        ;

scenario:
        'scenario' name=ID '(' ')' ver=version '{'
        instruction*
        '}'
        ;

subScenario:
        'scenario' name=ID '(' variableDeclaration (',' variableDeclaration)* ')' ver=version '{'
        instruction*
        '}'
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
        send | receive | check | group | execute
        ;

send:
        (stream=ID ':=')? ch=channel '<<' protobuf
        ;

receive:
        ch=channel '>>' protobuf
        ;

channel:
        'gnmi' | 'ctrl'
        ;

check:
        checkRegex | checkUnique
        ;

checkRegex:
        'check' 'regex' 'match' '(' variable ',' (variable|constant) ')'
        ;

checkUnique:
        'check' 'unique' '(' variable (',' variable)+ ')'
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
        name=ID '{' protobufField* '}'
        ;

protobufField:
        protobufFieldSimple | protobufFieldGroup | protobufFieldRepeated
        ;

protobufFieldSimple:
        name=ID ':' (val_number=NUMBER | val_string=STRING | val_enum=ID | val_var=variable | val_path=path)
        ;

protobufFieldGroup:
        name=ID '{' protobufField* '}'
        ;

protobufFieldRepeated:
        name=ID '[' protobufFieldRepeatedRow+ ']'
        ;

protobufFieldRepeatedRow:
        '{' protobufField+ '}' ','?
        ;

path:
        ('/' pathElement)*
        ;

pathElement:
        name=ID ('[' key=ID '=' (a='*'|v=variable|e=ID) ']')?| param=variable
        ;

ID: [a-zA-Z_][a-zA-Z0-9_-]* ;
WS: [ \t\r\n]+ -> skip ;
LINE_COMMENT: '//' ~( '\r' | '\n')* -> skip ;
BLOCK_COMMENT: '/*' .*? '*/' -> skip ;
STRING: '"' .+? '"' ;
NUMBER: '-'? INT ;
fragment INT: '0' | [1-9] [0-9]* ;
