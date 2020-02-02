# rekicc
## What"s this ?

This is an experimental tiny C language compiler inspired by [9cc](https://github.com/rui314/9cc). Will grow toward enabing self-compile.

## Usage

run `make` and you get executed result of `test/test.c`

## Current BNF
```
program: function* eof

function: type func_name '(' var_declare? ( ',' var_declare )* ')' block

type: ( struct_type | premitive_type ) '*'*
premitive_type: 'int' | 'char' | 'long'
struct_type: 'struct' ( struct_declare | struct_name )
struct_declare: struct_name? '{' member_decrare* '}'
member_declare: type member_name array_type? ';'
struct_name: IDENT

var_declare: type var_name array_type? ';'
array_type: '[' int? ']'

block: '{' var_delcare* ( block_or_statement )* '}' <eof>
block_or_statement: ( statement | block )

statement: ';' | print_statement | if_statement | while_statement | for_statement | do_while_statement | expr_statement | return_statement
if_statement: 'if' '(' expr ')' ( statement | block ( 'else' block_or_statement )? )
for_statement: 'for' '(' expr ';' expr ';' expr ')' block_or_statement
while_statement: 'while' '(' expr ')' block_or_statement
do_while_statement: 'do' block 'while' '(' expr ')' ';'
print_statement: 'print' '(' expr ');'
return_statement: 'return' expr ';'
expr_statement: expr ';'

expr: value ( '=' expr )*

value: logical_or
logical_or: logical_and ( '||' logical_and )*
logical_and: equality ( '&&' equality )*
equality: lessgreat ( ( '==' | '!=' ) lessgreat )*
lessgreat: add ( ( '<' | '<=' | '>' | '>=' ) add )*
add: mul ( ( '+' | '-' ) mul )*
mul: unary ( ( '*' | '/' | '%' ) unary )*

unary: prefix

prefix: postfix | logical_not | signed | ptr | ptr_deref | prefix_incdec
logical_not: '!' prefix
signed ( '+' | '-' ) prefix
ptr: '&' var_name
ptr_deref: '*' prefix
prefix_incdec: ( '++' | '--' ) var_name

postfix: primary | apply_func | struct_member | postfix_array | postfix_incdec
apply_func: func_name '(' expr? ( ',' expr )* ')'
postfix_array: postfix '[' expr ']'
postfix_incdec: postfix ( '++' | '--' )
struct_member: postfix '.' member_name | postfix '->' member_name

primary: var_name | literal | '(' expr ')'

literal: signed_int | int | char | global_string
global_string: '"' escaped_string '"'
signed_int: ( '+' | '-' ) int
char: ''' ( ANY | escaped_char ) '''

escaped_string: ( ANY | escaped_char )*
escaped_char: '\' [abfnrtr"'\]
func_name: IDENT
var_name: IDENT
int: DIGIT*

IDENT: IDENT_CHAR ( IDENT_CHAR | DIGIT )*;
IDENT_CHAR: ALPHA | '_'
ALPHA: [A-za-z]
DIGIT: [0-9]
```
