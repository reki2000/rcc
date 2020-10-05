# x64 AT&T assemly syntax for each operand sizes

## registers

|8|4|2|1|
|-|-|-|-|
|%rax|%eax|%ax|%al|
|%rbx|%ebx|%bx|%bl|
|%rcx|%ecx|%cx|%cl|
|%rdx|%ecx|%dx|%dl|
|%rdi|%edi|%di|%dil|
|%rsi|%esi|%si|%sil|
|%rbp|%ebp|%bp|%bpl|
|%rsp|%esp|%sp|%spl|
|%r8|%r8d|%r8w|%r8l|
|%r15|%r15d|%r15w|%r15l|

## instructions

|8|4|2|1|
|-|-|-|-|
|movq|movl|movw|movb|
|addq|addl|addw|addb|
|subq|subl|subw|subb|


# How type information is stored

## AST
```
typedef struct {
    int type;
    type_t *t;
    union {
        char *ptr_value;
        int int_value;
        char char_value;
        long long_value;
        int atom_pos;
    };
    int token_pos;
} atom_t;
```

```
char a;      //  0  t:"char"
char *b;     //  1  t:ptr_of --> "char", length = -1
char c[100]; //  5  t:ptr_of --> "char", length = 100

a;
RVALUE( 
    atom:VAR_REF(0, t:{*char"}),
    t:{char} // remove ptr
)

b;
RVALUE(
    atom:VAR_REF(0, t:{**char}),
    t:{*char} // remove ptr
)

*b;
RVALUE(
    DEREF( 
        RVALUE(
            atom:VAR_REF(0, t:{**char})
            t:{*char}
        )
        t:{char}
    }
    t:{char}  // keep type
)

&a; 
RVALUE(
    PTR_OF(
        atom:VAR_REF(0, t:{*char}),
        t:{*char}
    )
    t:{*char} // keep type
}

c;
RVALUE(
    atom:VAR_REF(offset:5, t:{[100]char}, 
    t:{*char} // convert from array to pointer
)

&c;
// it's invalid -- array cannot take its pointer
// or same as 'c'
RVALUE(
    atom:VAR_REF(offset:5, t:{[100]char}, 
    t:{*char} // convert from array to pointer
)

*c;
// it's invalid -- array cannot take direct dereference
// or same as 'c'
RVALUE(
    atom:VAR_REF(offset:5, t:{[100]char}, 
    t:{*char} // convert from array to pointer
)
```

# AST for multi-dimension array
```
int a[3][5];
a[1][3];
```
| a[0][0] | a[0][1] | a[0][2] | a[0][3] | a[0][4] |
| a[1][0] | a[1][1] | a[1][2] | a[1][3] | a[1][4] |
| a[2][0] | a[2][1] | a[2][2] | a[2][3] | a[2][4] |

INDEX(
    base:INDEX(
            base:VAR_REF(
                    offset: 0 // a
                    type:{[3][5]int}
                ),
            index:1,
            type:{[5]int}
        ),
    index:3,
    type:{*int}
)


## va_list

  -0xd0: va_list.gp_offset (-208) va_list
  -0xcc: va_list.fp_offset (-204)
  -0xc8: va_list.reg_save_area (-200)
  -0xc0: va_list.overflow_arg_area (-192)
  -0xb8: va_list va; (*struct __buitin_va_list) (-184)
  -0xb0: gp_save_area: %rdi (-176)
  -0xa8: gp_save_area: %rsi (-168)
  -0xa0: gp_save_area: %rdx
  -0x98: gp_save_area: %rcx 
  -0x90: gp_save_area: %r8
  -0x88: gp_save_area: %r9 (-132)
  -0x80: fp_save_area: xmm0 (-128)
  -0x70: fp_save_area: xmm1
  -0x60: fp_save_area: xmm2
  -0x50: fp_save_area: xmm3
  -0x40: fp_save_area: xmm4
  -0x30: fp_save_area: xmm5
  -0x20: fp_save_area: xmm6
  -0x10: fp_save_area: xmm7
  -0x08: alignment
- -0x04: first arg  (int)
-     0: pushed %rbp  <-- initial %rbp
- +0x08: callar_address
- +0x10: pushed args

## Pre processor specification

- '\' at the end of line is 'transparent eol - all C source including pre-processor cannot understand this as an EOL
- /* .. */ comment (cannot be nested)
- // comment 
- '#' should be the first character of the line. only whole the line is its body. the last '\' in the line tells that the next line is the continuing the previous line.
- #ifdef .. #else ... #endif  can be nested
```
define: start_of_line '#' 'define' IDENT ( '(' IDENT ( ',' IDENT )* ) ')' )? ( tokens )* end_of_line
```

