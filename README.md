# rcc

Rcc is a tiny subset of C language compiler inspired by [9cc](https://github.com/rui314/9cc). Supports minimum language features enough to compile itself.

## Architecture Overview

- Subset of C89
- Compiling stages: tokenize, parse, output-x64|aarch64-asm
- LL(1) hand-written parser
- Data type model: x64 - LP64 (int:32, long:64, pointer:64)
- Register machine with simple register assigmnent logic (round-robin within a single expression)
- Depends on external assembler (as) and linker (ld)
- Highly limited use of C standard library (listed in `include/rsys.h`)

## Language features *NOT* supported yet

- unsigned types (all numeric variables are handled as signed)
- floating point types
- struct type return value
- struct initializer
- 'dynamic function' call
- goto
- some macros - __FILE__ __LINE__ 
- error handling for invalid syntax source codes
- etc.

## How to build and test

```
# build & test gen1 - compiled by gcc
make test

# build & test gen2 - complied by gen1 compiler
make test-gen2

# build & test gen3 - compiled by gen2 compiler
make test-gen3
```

## How to build and test AArch64 version on x64 Linux

* aarch64 support is under development

```
docker build -t arm64 arm64

make arm
test/test_arm.sh
```

## Current BNF
```
#
# ? : occurs 0 or 1
# * : occurs 0 or more
# | : or - at least one of the items on the right or left of this, are required
# ( .. ) : grouping for ? or * or |
# small_case : token (spaces between items require one or more delimiters)
# UPPER_CASE : sequence of single character (spaces between items mean that these items are in the series of characters in one token, not delimited )
#

program: global_declaration* eof

global_declaration: storage_class? type_declaration ( ';' | global_variable ';' | function_prototype ';' | function_definition )

storage_class: 'extern'

const_class: 'const'

type_declaration: typedef | union_or_struct_type | enum_type | defined_type | primitive_type

typedef: 'typedef' type_declaration pointer? defined_type
defined_type: IDENT

global_variable: pointer? var_name array_type? ( '=' ( global_array_initializer | literal ) )?
global_array_initializer: '{' literal+ '}'

function_prototype: pointer? func_name '(' function_prototype_arg ( ',' function_prototype_arg )* ')' ';'
function_prototype_arg: const_class? type_declaration pointer? ( var_name array_type? )? 

function_definition: pointer? func_name '(' var_declare? ( ',' var_declare )* ')' block

pointer: '*'+
primitive_type: 'int' | 'char' | 'long'
array_type: ( '[' int? ']' )+

enum_declarator: 
enum_type: 'enum' ( enum_name? enum_declare | enum_name )
enum_name: IDENT
enum_declare: '{' enum_member ( ',' enum_member )* '}'
enum_member: enum_member_name ( '=' int_literal_expr )? 
enum_member_name: IDENT

union_or_struct_type: ( 'struct' | 'union' ) ( struct_name? struct_declare | struct_name )
struct_declare: '{' (union_in_struct | struct_member_decrare)* '}'
union_in_struct: 'union' '{' struct_member_decrare* '}' ';'
struct_member_declare: type_declaration pointer member_name array_type? ';'
struct_name: IDENT

var_declare: const_class? type_declaration pointer? var_identifier ( ',' var_identifier )* ';'
var_identifiers: var_name array_type? local_variable_initializer?

block: '{' block_or_statement* '}'
block_or_statement: ( statement | block )

statement: ';' | var_declare | print_statement | if_statement | while_statement | for_statement 
  do_while_statement | expr_statement | return_statement | break_statement | continue_statement | switch_statement

if_statement: 'if' '(' expr_sequence ')' ( statement | block ( 'else' block_or_statement )? )
for_statement: 'for' '(' (var_declare | expr_statement) expr_sequence ';' expr_sequence ')' block_or_statement
while_statement: 'while' '(' expr_sequence ')' block_or_statement
do_while_statement: 'do' block 'while' '(' expr_sequence ')' ';'
print_statement: 'print' '(' expr ');'
return_statement: 'return' expr_sequence? ';'
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
local_variable_initializer: '=' ( array_initializer | expr )
array_initializer: '{' expr ( ',' expr )* '}'
postfix_assignment: postfix ( '+= ' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' | '<<=' | '>>=' | '~=' ) expr

value: logical_or

logical_or: logical_and ( '||' logical_and )*
logical_and: bitwise_or ( '&&' bitwise_or )*

bitwise_or: bitwise_xor ( '|' bitwise_xor )*
bitwise_xor: bitwise_and ( '^' bitwise_and )*
bitwise_and: equality ( '|' equality )*

equality: lessgreat ( ( '==' | '!=' ) lessgreat )*
lessgreat: bitshift ( ( '<' | '<=' | '>' | '>=' ) bitshift )*
bitshift: add ( ( '<<' | '>>' ) add )*
add: mul ( ( '+' | '-' ) mul )*
mul: unary ( ( '*' | '/' | '%' ) unary )*

unary: prefix

prefix: postfix | cast | prefix_incdec | logical_not | bitwise_not | signed | ptr | ptr_deref | sizeof

cast : '(' type_declaration pointer? ')' unary
logical_not: '!' unary
bitwise_not: '~' unary
signed: ( '+' | '-' ) unary
sizeof: 'sizeof' ( unary | type_name )
ptr: '&' unary
ptr_deref: '*' unary
prefix_incdec: ( '++' | '--' ) var_name

postfix: primary | apply_func | struct_member | postfix_array | postfix_incdec
apply_func: func_name '(' expr? ( ',' expr )* ')'
postfix_array: postfix '[' expr ']'
postfix_incdec: postfix ( '++' | '--' )
struct_member: postfix '.' member_name | postfix '->' member_name

primary: var_name | literal | '(' expr ')'

literal: INT | CHAR | enum_member_name | GLOBAL_STRING
func_name: IDENT
var_name: IDENT

CHAR: ''' ( ANY | ESCAPED_CHAR ) '''

GLOBAL_STRING: '"' ESCAPED_STRING '"'

ESCAPED_STRING: ( ANY | ESCAPED_CHAR )*
ESCAPED_CHAR: '\' [0abfnrtr"'\]

INT: ( HEX | OCT | DECIMAL ) 'L'?

HEX: '0x' [0-9A-Fa-f]+
OCT: '0' DIGIT*
DECIMAL: [1-9] DIGIT*

IDENT: IDENT_CHAR ( IDENT_CHAR | DIGIT )*;
IDENT_CHAR: ALPHA | '_'
ALPHA: [A-za-z]
DIGIT: [0-9]
```
