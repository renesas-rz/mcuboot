/* Shim for TF-A <drivers/delay_timer.h>: busy-wait on the Arm generic timer (CNTPCT_EL0).
 * The IPL starts the system counter (SYC) and runs at EL3, so CNTFRQ_EL0 should be set.
 * If it reads 0 on your board, define RZA3M_CNTFRQ_HZ with the SYC frequency. */
#ifndef IPL_SHIM_DELAY_TIMER_H
#define IPL_SHIM_DELAY_TIMER_H
#include <stdint.h>
static inline uint64_t ipl_shim_cntfrq(void)
{
#ifdef RZA3M_CNTFRQ_HZ
    return RZA3M_CNTFRQ_HZ;
#else
    uint64_t f; __asm volatile("mrs %0, cntfrq_el0" : "=r"(f)); return f;
#endif
}
static inline void udelay(uint32_t us)
{
    uint64_t t0, t, ticks = (ipl_shim_cntfrq() * us + 999999U) / 1000000U;
    __asm volatile("isb; mrs %0, cntpct_el0" : "=r"(t0));
    do { __asm volatile("isb; mrs %0, cntpct_el0" : "=r"(t)); } while ((t - t0) < ticks);
}
static inline void mdelay(uint32_t ms) { while (ms--) { udelay(1000U); } }
#endif
