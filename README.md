# rekicc
## What"s this ?

This is an experimental tiny C language compiler inspired by [9cc](https://github.com/rui314/9cc). Will grow toward enabing self-compile.

## Usage

run `make` and you get executed result of `test/test.c`

## Current BNF
```
block = '{' var_delcare* ( statement | block )* '}' <eof>
var_declare = 'int' var_name ';'
statement = ';' | print_statement | expr_statement
print_statement = 'print' expr ';'
expr_statement = expr ';'

expr = lvalue '=' rvalue | rvalue
lvalue = var_ref

rvalue = logical_or
logical_or = logical_and ( '||' logical_and )*
logical_and = equality ( '&&' equality )*
equality = lessgreat ( ( '==' | '!=' ) lessgreat )*
lessgreat = add ( ( '<' | '<=' | '>' | '>=' ) add )*
add = mul ( ( '+' | '-' ) mul )*
mul = not ( ( '*' | '/' | '%' ) not )*
not = primary | '!' primary
primary = int | var_ref | '(' expr ')'

var_ref = IDENT
var_name = IDENT
int = DIGIT*

IDENT = IDENT_CHAR ( IDENT_CHAR | DIGIT )*;
IDENT_CHAR = ALPHA | '_'
ALPHA = [A-za-z]
DIGIT = [0-9]
```
