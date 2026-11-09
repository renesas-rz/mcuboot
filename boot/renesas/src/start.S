/*
 * RZ/A3M MCUboot port - entry point. The stock IPL copies this image to
 * RZA3M_MCUBOOT_DDR_BASE and branches here (AArch64, EL3 per the IPL readme).
 * The IPL's MMU/cache state is not assumed: clean caches, run MMU off, I-cache on.
 */
    .section .text.entry, "ax"
    .global _start
_start:
    msr     daifset, #0xf
    bl      rza3m_dcache_clean_inv_all      /* uses no stack */

    ldr     x2, =rza3m_vectors
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #3
    b.ne    1f
    mrs     x1, sctlr_el3
    bic     x1, x1, #0x1                    /* M off */
    bic     x1, x1, #0x4                    /* C off */
    bic     x1, x1, #0x2                    /* A off */
    orr     x1, x1, #0x1000                 /* I on  */
    msr     sctlr_el3, x1
    isb
    msr     vbar_el3, x2
    isb
    tlbi    alle3
    b       2f
1:  mrs     x1, sctlr_el1
    bic     x1, x1, #0x1
    bic     x1, x1, #0x4
    bic     x1, x1, #0x2
    orr     x1, x1, #0x1000
    msr     sctlr_el1, x1
    isb
    msr     vbar_el1, x2
    isb
    tlbi    vmalle1
2:  ic      iallu
    dsb     sy
    isb

    ldr     x0, =__stack_top
    mov     sp, x0

    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
3:  cmp     x0, x1
    b.hs    4f
    str     xzr, [x0], #8
    b       3b
4:  bl      rza3m_mcuboot_main
5:  wfe
    b       5b

/* ---- vectors: each entry records its index in x9, then jumps to the trap ---- */
    .macro  VEC idx
    .balign 128
    mov     x9, #\idx
    b       rza3m_exception_trap
    .endm

    .section .text.vectors, "ax"
    .balign 2048
    .global rza3m_vectors
rza3m_vectors:
    VEC 0
    VEC 1
    VEC 2
    VEC 3
    VEC 4
    VEC 5
    VEC 6
    VEC 7
    VEC 8
    VEC 9
    VEC 10
    VEC 11
    VEC 12
    VEC 13
    VEC 14
    VEC 15

    .global rza3m_exception_trap
rza3m_exception_trap:
    mov     x10, sp                         /* SP at the time of the fault */
    ldr     x11, =__exc_stack_top           /* own stack, SP may be broken */
    mov     sp, x11
    mov     x6, x30                         /* LR at the time of the fault */
    mov     x5, x10
    mov     x4, x9                          /* vector index */
    mrs     x0, CurrentEL
    cmp     x0, #(3 << 2)
    b.ne    6f
    mrs     x0, esr_el3
    mrs     x1, elr_el3
    mrs     x2, far_el3
    mrs     x3, spsr_el3
    b       7f
6:  mrs     x0, esr_el1
    mrs     x1, elr_el1
    mrs     x2, far_el1
    mrs     x3, spsr_el1
7:  bl      rza3m_exception_report          /* prints over the polled console */
    .global rza3m_exception_dead
rza3m_exception_dead:                       /* hbreak here if a debugger is attached */
    wfe
    b       rza3m_exception_dead
    .ltorg

    .section .bss.rza3m_exc_stack, "aw", %nobits
    .balign 16
    .space  4096
__exc_stack_top:
