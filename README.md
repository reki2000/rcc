# rekicc
## What"s this ?

This is an experimental tiny C language compiler inspired by [9cc](https://github.com/rui314/9cc). Will grow toward enabing self-compile.

## Usage

run `make` and you get executed result of `test/test.c`

## Current BNF
```
program: function*

function: 'int' '*'? func_name '(' var_declare? ( ',' var_declare )* ')' block

block: '{' var_delcare* ( block_or_statement )* '}' <eof>
block_or_statement: = ( statement | block )
var_declare: 'int' '*'? var_name ';'

statement: ';' | print_statement | if_statement | while_statement | for_statement | do_while_statement | expr_statement | return_statement
if_statement: 'if' '(' expr ')' ( statement | block ( 'else' block_or_statement )? )
for_statement: 'for' '(' expr ';' expr ';' expr ')' block_or_statement
while_statement: 'while' '(' expr ')' block_or_statement
do_while_statement: 'do' block 'while' '(' expr ')' ';'
print_statement: 'print' expr ';'
return_statement: 'return' expr ';'
expr_statement: expr ';'

expr: value ( '=' value )?

value: logical_or
logical_or: logical_and ( '||' logical_and )*
logical_and: equality ( '&&' equality )*
equality: lessgreat ( ( '==' | '!=' ) lessgreat )*
lessgreat: add ( ( '<' | '<=' | '>' | '>=' ) add )*
add: mul ( ( '+' | '-' ) mul )*
mul: primary ( ( '*' | '/' | '%' ) primary )*

primary: literal | unary
unary: literal | not | ref | '(' expr ')'
not: '!' unary

ref: ptr | ptr_deref | var | apply_func
apply_func: func_name '(' expr? ( ',' expr )* ')'

var: var_name
ptr: '&' var_name
ptr_deref: '*' var_name

literal: int

func_name: IDENT
var_name: IDENT
int: DIGIT*

IDENT: IDENT_CHAR ( IDENT_CHAR | DIGIT )*;
IDENT_CHAR: ALPHA | '_'
ALPHA: [A-za-z]
DIGIT: [0-9]
```
