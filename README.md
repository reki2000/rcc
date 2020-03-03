# rekicc
## What"s this ?

This is an experimental tiny C language compiler inspired by [9cc](https://github.com/rui314/9cc). Will grow toward enabing self-compile.

## Usage

run `make` and you get executed result of `test/test.c`

## Current BNF
```
program: global_declaration* eof

global_declaration: type_declaration ( ';' | global_variable ';' | function_definition )

global_variable: pointer? var_name array_type? ( '=' literal )?

type_declaration: typedef | union_or_struct_type | enum_type | defined_type | primitive_type

typedef: 'typedef' type_declaration pointer? defined_type
defined_type: IDENT


function_definition: pointer? func_name '(' var_declare? ( ',' var_declare )* ')' block

pointer: '*'+
primitive_type: 'int' | 'char' | 'long'
array_type: ( '[' int? ']' )+

enum_declarator: 
enum_type: 'enum' ( enum_name? enum_declare | enum_name )
enum_name: IDENT
enum_declare: '{' enum_member ( ',' enum_member )* '}'
enum_member: member_name ( '=' int )? 

union_or_struct_type: ( 'struct' | 'union' ) ( struct_name? struct_declare | struct_name )
struct_declare: '{' struct_member_decrare* '}'
struct_member_declare: type_declaration pointer member_name array_type? ';'
struct_name: IDENT

var_declare: type_declaration pointer var_name array_type? ( assignment )? ';'

block: '{' var_delcare* ( block_or_statement )* '}'
block_or_statement: ( statement | block )

statement: ';' | print_statement | if_statement | while_statement | for_statement | do_while_statement | expr_statement | return_statement | break_statement | continue_statement | switch_statement
if_statement: 'if' '(' expr_sequence ')' ( statement | block ( 'else' block_or_statement )? )
for_statement: 'for' '(' expr_sequence ';' expr_sequence ';' expr_sequence ')' block_or_statement
while_statement: 'while' '(' expr_sequence ')' block_or_statement
do_while_statement: 'do' block 'while' '(' expr_sequence ')' ';'
print_statement: 'print' '(' expr ');'
return_statement: 'return' expr_sequence ';'
break_statement: 'break' ';'
continue_statement: 'continue' ';'
switch_statement: 'switch' '(' expr_sequence ')' '{' case_clause* default_clause? '}'
case_clause: 'case' int_literal ':' statement*
default_clause: 'default' ':' statement*
expr_statement: expr_sequence ';'

expr_sequence: expr ( ',' expr )*
expr: value ( ternary | assignment | postfix_assignment )* 
ternaly: '?' expr ':' expr
assignment: '=' expr
postfix_assignment: ( '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' ) expr

value: logical_or

logical_or: logical_and ( '||' logical_and )*
logical_and: equality ( '&&' equality )*
equality: lessgreat ( ( '==' | '!=' ) lessgreat )*
lessgreat: add ( ( '<' | '<=' | '>' | '>=' ) add )*
add: mul ( ( '+' | '-' ) mul )*
mul: unary ( ( '*' | '/' | '%' ) unary )*

unary: prefix

prefix: postfix | prefix_incdec | logical_not | signed | ptr | ptr_deref | sizeof
logical_not: '!' prefix
signed: ( '+' | '-' ) prefix
sizeof: 'sizeof' ( unary | type_name )
ptr: '&' unary
ptr_deref: '*' unary
prefix_incdec: ( '++' | '--' ) var_name

postfix: primary | apply_func | struct_member | postfix_array | postfix_incdec | postfix_assignment
apply_func: func_name '(' expr? ( ',' expr )* ')'
postfix_array: postfix '[' expr ']'
postfix_incdec: postfix ( '++' | '--' )
postfix_assignment: postfix ( '+= ' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' ) expr
struct_member: postfix '.' member_name | postfix '->' member_name

primary: var_name | literal | '(' expr ')'

literal: int_literal | global_string
int_literal: signed_int | int | char
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
