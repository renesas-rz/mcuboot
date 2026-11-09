/*
 * RZ/A3M MCUboot port - cache maintenance and AArch64 handoff.
 * Handoff state to the FreeRTOS app: same EL as MCUboot (EL3 from the IPL), DAIF masked,
 * D-cache cleaned+invalidated to PoC, MMU/D-cache/I-cache off, I-cache + TLBs invalidated.
 */
    .text
    .balign 4

/* void rza3m_dcache_clean_inv_all(void) - set/way, all levels up to LoC. No stack. Clobbers x0-x11. */
    .global rza3m_dcache_clean_inv_all
    .type   rza3m_dcache_clean_inv_all, %function
rza3m_dcache_clean_inv_all:
    mrs     x0, clidr_el1
    ubfx    w3, w0, #24, #3
    lsl     w3, w3, #1
    cbz     w3, 5f
    mov     w10, #0
1:  add     w2, w10, w10, lsr #1
    lsr     w1, w0, w2
    and     w1, w1, #7
    cmp     w1, #2
    b.lt    4f
    msr     csselr_el1, x10
    isb
    mrs     x1, ccsidr_el1
    and     w2, w1, #7
    add     w2, w2, #4
    ubfx    w4, w1, #3, #10
    clz     w5, w4
    ubfx    w7, w1, #13, #15
2:  mov     w9, w4
3:  lsl     w6, w9, w5
    orr     w11, w10, w6
    lsl     w6, w7, w2
    orr     w11, w11, w6
    dc      cisw, x11
    subs    w9, w9, #1
    b.ge    3b
    subs    w7, w7, #1
    b.ge    2b
4:  add     w10, w10, #2
    cmp     w3, w10
    b.gt    1b
5:  mov     x10, #0
    msr     csselr_el1, x10
    dsb     sy
    isb
    ret
    .size rza3m_dcache_clean_inv_all, . - rza3m_dcache_clean_inv_all

/* void rza3m_disable_mmu_and_jump(uintptr_t entry) */
    .global rza3m_disable_mmu_and_jump
    .type   rza3m_disable_mmu_and_jump, %function
rza3m_disable_mmu_and_jump:
    mov     x19, x0
    msr     daifset, #0xf
    bl      rza3m_dcache_clean_inv_all
    mrs     x2, CurrentEL
    lsr     x2, x2, #2
    cmp     x2, #3
    b.ne    10f
    mrs     x1, sctlr_el3
    bic     x1, x1, #0x1
    bic     x1, x1, #0x4
    bic     x1, x1, #0x1000
    msr     sctlr_el3, x1
    isb
    bl      rza3m_dcache_clean_inv_all
    ic      iallu
    tlbi    alle3
    b       20f
10: mrs     x1, sctlr_el1
    bic     x1, x1, #0x1
    bic     x1, x1, #0x4
    bic     x1, x1, #0x1000
    msr     sctlr_el1, x1
    isb
    bl      rza3m_dcache_clean_inv_all
    ic      iallu
    tlbi    vmalle1
20: dsb     sy
    isb
    br      x19
    .size rza3m_disable_mmu_and_jump, . - rza3m_disable_mmu_and_jump
