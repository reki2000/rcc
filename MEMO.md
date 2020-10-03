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


- +0x10: pushed args
- +0x08: callar_address
-     0: pushed %rbp  <-- initial %rbp
- -0x04: first arg 
  -0x08: alignment
  -0x10: fp_save_area: 8
  -0x18: fp_save_area: 7
  -0x20: fp_save_area: 6
  -0x28: fp_save_area: 5
  -0x30: fp_save_area: 4
  -0x38: fp_save_area: 3
  -0x40: fp_save_area: 2
  -0x48: fp_save_area: 1
  -0x50: gp_save_area: %r9 (-80)
  -0x58: gp_save_area: %r8
  -0x60: gp_save_area: %rcx (-96)
  -0x68: gp_save_area: %rdx
  -0x70: gp_save_area: %rsi (-112)
  -0x78: gp_save_area: %rdi (-120)
  -0x80: va_list va; (*struct __buitin_va_list) (-128)
  -0x88: va_list.overflow_arg_area (-136)
  -0x90: va_list.reg_save_area (-144)
  -0x94: va_list.fp_offset (-148)
  -0x98: va_list.gp_offset (-152) va_list
