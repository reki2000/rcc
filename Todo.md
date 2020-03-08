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

- [*] operators += -= *= /= %= |= &= ^=
- [*] multiple expressions with comma

- [*] local variable initialization
- [*] global variable initialization for int

- [*] union
- [*] enum

- [*] typedef
- [*] enum declaration
- [*] union/struct declaration

- [*] sizeof

- [*] break, continue

- [*] switch case
- [*] ternary operator ?:

- [*] function prototype declaration

- [*] extern variable 
  - w/ extern
   - w/ init : .data, @global, WARN: 'initialized and declared 'extern'
   - w/o init : no declaration, only reference in code, duplicate OK
  - w/o extern : 
   - w/ init : .data, @global, duplicate OK, duplicate init NG
   - w/o init : .comm, not @global, duplicate OK

- [*] extern function

- [ ] inline comment
- [ ] multiline comment

- [ ] include

- [ ] compiler argument (-S)? (-o outfile)? {source.c)+

## A.D. (self hosted)

- [ ] bit operators >> >>= << <<= & | ^  ~

- [ ] cast
- [ ] implicit cast between char -> int, int -> char (signed)

- [ ] define value
- [ ] ifdef
- [ ] define macro

- [ ] array initialization
- [ ] local multiple variable definition
- [ ] global multiple variable definition

- [ ] long literal (10000L)
- [ ] hex literal (0x0000)

- [ ] goto

- [ ] float, double, double double, short, long, long long
- [ ] unsigned


- [ ] function declaration for > 6 args
- [ ] function call with > 6 args
- [ ] function call ABI alignment

- [ ] function with va_args

- [ ] static

- [ ] atom_t refactoring 2
 - use atom_t * instead of pos
 - rename TYPE to *

- [ ] struct assignment - memcpy(&a,&b,sizeof(a))
