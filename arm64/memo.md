# install

```
# Install Docker for Windows
docker build -t arm64 .
```

# run

```
docker run -v $(pwd)/work:/work --rm -ti arm64 /bin/bash -c \
  "cd work; aarch64-linux-gnu-gcc-8 -S -c *.c"
```

```
docker run -v $(pwd)/work:/work --rm -ti arm64 /bin/bash -c \
  "cd work; qemu-arm-static $*"
```

# references

AArch64 assembler の日本語の分かりやすい解説
https://www.mztn.org/dragon/arm6405str.html

AArch64 ABI
https://developer.arm.com/documentation/ihi0055/b/

# arm64

## branch operation
- b label (jmp)
- bl label (call) use x30
- br reg (jmp)
- blr reg (call) use x30

- adr (lea)
- adrp (lea 4kb aligned)


# ABI summary

## registers
- SP : stack spointer
- r30 : Link Register
- r29 : Frame Pointer
- r19..r28 : Callee Saved
- r18 : special (Platform) , temporary
- r17 : special (PLT temporary), temporary
- r16 : special (PLT temporary), temporary
- r9..r15 : temporary
- r8 : indirect result locaion
- r0..r7 : Parameter/result registers



# sample codes

```
        .arch armv8-a
        .file   "fib.c"
        .text
        .align  2
        .global fib
        .type   fib, %function
fib:
.LFB0:
        .cfi_startproc
        stp     x29, x30, [sp, -48]!  # stp: save 2 regs to address. '!' means pre-index - add offset to base reg BEFORE read from it
        .cfi_def_cfa_offset 48
        .cfi_offset 29, -48
        .cfi_offset 30, -40
        mov     x29, sp               # x29 <= sp
        str     x19, [sp, 16]         # x19 => sp+16 (store)
        .cfi_offset 19, -32
        str     w0, [sp, 44]          # w0 => sp+44 : -4 from 'at just called' sp
        ldr     w0, [sp, 44]          # w0 <= n
        cmp     w0, 2                 # w0 <= 2 ?
        ble     .L2
        ldr     w0, [sp, 44]          # w0 <= n
        sub     w0, w0, #1            # w0 = n-1
        bl      fib                   # call fib
        mov     w19, w0               # w19 <= w0 (return value)
        ldr     w0, [sp, 44]          # w0 <= n
        sub     w0, w0, #2            # w0 = n-2
        bl      fib                   # call fib
        add     w0, w19, w0           # w0 <= w0 + w19 (return value)
        b       .L4
.L2:
        mov     w0, 1                 # return 1
.L4:
        ldr     x19, [sp, 16]         # x19 <= sp+16 (restore)
        ldp     x29, x30, [sp], 48    # restore x29, x30, sp
        .cfi_restore 30
        .cfi_restore 29
        .cfi_restore 19
        .cfi_def_cfa_offset 0
        ret                           # return
        .cfi_endproc
.LFE0:
        .size   fib, .-fib
        .section        .rodata
        .align  3
.LC0:
        .string "%d\n"
        .text
        .align  2
        .global main
        .type   main, %function
main:
.LFB1:
        .cfi_startproc
        stp     x29, x30, [sp, -32]!
        .cfi_def_cfa_offset 32
        .cfi_offset 29, -32
        .cfi_offset 30, -24
        mov     x29, sp
        mov     w0, 10
        str     w0, [sp, 28]
        ldr     w0, [sp, 28]
        bl      fib
        mov     w1, w0                # w1 - 2nd arg
        adrp    x0, .LC0              # ready for x0 to set .LC0 page
        add     x0, x0, :lo12:.LC0    # w0 - first arg, added offset .LC0
        bl      printf
        mov     w0, 0
        ldp     x29, x30, [sp], 32
        .cfi_restore 30
        .cfi_restore 29
        .cfi_def_cfa_offset 0
        ret
        .cfi_endproc
.LFE1:
        .size   main, .-main
        .ident  "GCC: (Debian 8.3.0-2) 8.3.0"
        .section        .note.GNU-stack,"",@progbits
```

