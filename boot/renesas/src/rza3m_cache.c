/* RZ/A3M MCUboot port - range cache maintenance (AArch64) */
#include "rza3m_port.h"

void rza3m_dcache_clean_inv_range(uintptr_t start, size_t len)
{
    uint64_t ctr;
    __asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uintptr_t line = (uintptr_t)4U << ((ctr >> 16) & 0xFU);
    uintptr_t end  = start + len;
    for (uintptr_t a = start & ~(line - 1U); a < end; a += line) {
        __asm volatile("dc civac, %0" : : "r"(a) : "memory");
    }
    __asm volatile("dsb sy" : : : "memory");
}
