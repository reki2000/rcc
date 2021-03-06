# Todo

## B.C.

- [x] print integer
- [x] assignment expression

- [x] equality oeprators == != > < >= <=
- [x] logical operators && || !
- [x] tokernize

- [x] if statement
- [x] for statement
- [x] while statement
- [x] do-while statement

- [x] pointer
- [x] assignment using pointer

- [x] return
- [x] delcare function without args but returns a value
- [x] call no args function
- [x] declare function with args
- [x] call function with args

- [x] regression test

- [x] unary operators +,-
- [x] string literal
- [x] char
- [x] prefix ++, --
- [x] postfix ++, --

- [x] pointer + -
- [x] 1-dimension array

- [x] char literal

- [x] struct

- [x] global variable

- [x] atom_t refactoring 1
 - use a.value.* to a.*
 - organize atom functions

- [x] multi-dimension array

- [x] operators += -= *= /= %= |= &= ^=
- [x] multiple expressions with comma

- [x] local variable initialization
- [x] global variable initialization for int

- [x] union
- [x] enum

- [x] typedef
- [x] enum declaration
- [x] union/struct declaration

- [x] sizeof

- [x] break, continue

- [x] switch case
- [x] ternary operator ?:

- [x] function prototype declaration

- [x] extern variable 
  - w/ extern
   - w/ init : .data, @global, WARN: 'initialized and declared 'extern'
   - w/o init : no declaration, only reference in code, duplicate OK
  - w/o extern : 
   - w/ init : .data, @global, duplicate OK, duplicate init NG
   - w/o init : .comm, not @global, duplicate OK

- [x] extern function

- [x] inline comment
- [x] multiline comment

- [x] include

- [x] compiler options (-S)? 

- [x] refactor : remove _read, _write etc.

- [x] allow variable declaration in middle of block
- [x] array initialization


## New Era

- [x] const classifier (just ignoring)
- [x] anonymous union in struct
- [x] implicit cast for char -> int/long, int -> char/long, long- > char/int
- [x] implicit but warned cast for void* -> any pointers
- [x] add warning level for logging
- [x] local multiple variable definition
- [x] array initializer for non-sized array declaration
- [x] variable declaration in the first clause of 'for'
- [x] cast
- [x] global variable initializer for string literals
- [x] empty statement for 'for'
- [x] define value
- [x] test 066 (assignment from fixed-length-array to pointer) fails
- [x] compiler options (-o outfile)? {source.c)
- [x] bit operators >> >>= << <<= & | ^  ~
- [x] define macro

## bugs


## A.D. (self hosted)


- [ ] cli : allow multiple .c files

- [ ] signed char/int/long
- [ ] unsigned char/int/long

- [x] #ifdef, #ifndef, #else, #endif
- [x] #define macro
- [x] #undef
- [ ] '#' should be the first valid character in a line
- [x] ## operator
- [ ] # operator
- [ ] __FILE__, __LINE__
- [ ] __VA_ARGS__ # C99

- [ ] multiple global variable definition
- [x] array global variable initialization

- [x] long literal (10000L)
- [ ] concatinate sequence of string literal 
- [x] hex literal (ex:0x0000), octal literal(ex:0100)

- [ ] goto

- [ ] float, double, double double, short, long long

- [x] function declaration for > 6 args
- [x] function call with > 6 args
- [x] function call ABI stack 16bytes alignment

- [x] variadic function call/def
- [x] va_list type, va_start, va_end macro
- [x] va_arg

- [ ] 'static' variables in function
- [ ] 'static' global variables

- [x] struct assignment - memcpy(&a,&b,sizeof(a))
- [x] struct argument
- [ ] struct return value

- [ ] bug / expression sequence - evaluated result should reflect the last expression

## Refactorings

- [x] rewrite code generation from stack-machine to register-machine (!)

- [ ] atom_t refactoring 2
 - use atom_t * instead of pos
 - rename TYPE_ to K_(KIND_) / C_(CLASS_) / N_(NODE_) ...

- [x] remove print statement, replace with `void print(int i) { printf("%d\n",i); }`

- [ ] the parser shoud NOT consider ABI - variable offset should be calculated in emitting stage, not in parsing stage

- [ ] stop calculate variables' offset at parse stage, do it emission stage 
  - it's depends on ABI - the AST shouldn't know whether the variable is passed on stack or on registers
  - [ ] change va_arg's register save area at the begining -REG_ZREA_SIZE(%rbp) of the function's stack frame
