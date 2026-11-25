/*
 * RZ/A3M MCUboot port - watchdog (WDT channel 0), bare-metal, no FSP.
 *
 * Shared by MCUboot and the application (link the same file into both).
 *
 * Hardware (RZ/A3M HW manual R01UH1038EJ0110, chapter 21 and 7.2.4.131/132):
 *   - WDT CH0 base 0x1280_0800: WDTCNT +0x00, WDTSET +0x04, WDTTIM +0x08, WDTINT +0x0C
 *   - Period = (WDTTIME + 1) * 1024 * 1024 / 24 MHz  (43.69 ms steps, WDTSET[31:20])
 *   - 1st overflow: interrupt request, 2nd overflow: reset request
 *     -> effective timeout = 2 * period
 *   - Writing WDTINT.INTDISP = 1 clears the counter (= refresh)
 *   - Once WDTCNT.WDTEN = 1 the WDT cannot be stopped or reconfigured until reset
 *   - CPG_WDTRST_SEL.WDTRSTSEL0 = 1 turns the reset request into a system reset
 *   - CPG_WDTOVF_RST.WDTOVF0 = 1 after a WDT system reset (cleared by writing 1)
 *
 * Clock/reset of the WDT module are released by the IPL (cpg_wdtrst_sel_setup).
 */
#include <stdbool.h>
#include <stdint.h>
#include "rza3m_wdt.h"

/* ---- WDT CH0 ---- */
#define WDT0_BASE            0x12800800UL
#define WDT_WDTCNT           (WDT0_BASE + 0x00U)
#define WDT_WDTSET           (WDT0_BASE + 0x04U)
#define WDT_WDTINT           (WDT0_BASE + 0x0CU)
#define WDTCNT_WDTEN         (1UL << 0)
#define WDTINT_INTDISP       (1UL << 0)
#define WDTSET_WDTTIME_SHIFT 20U
#define WDTSET_WDTTIME_MAX   0xFFFU

/* CPG offsets and masks match the R9A07G066M FSP device headers. */
#define CPG_BASE             0x11010000UL
#define CPG_WDTOVF_RST       (CPG_BASE + 0xB10U)
#define CPG_WDTRST_SEL       (CPG_BASE + 0xB14U)
#define WDTOVF0              (1UL << 0)
#define WDTOVF0_WEN          (1UL << 16)
#define WDTRSTSEL0           (1UL << 0)
#define WDTRSTSEL0_WEN       (1UL << 16)

/* One period step in microseconds: 1024 * 1024 / 24 MHz = 43690.67 us */
#define WDT_STEP_US          43691U

#define REG32(a)             (*(volatile uint32_t *)(uintptr_t)(a))

/* Self-contained delay on the 24 MHz generic counter (CNTPCT_EL0). */
static void wdt_delay_us(uint32_t us)
{
    uint64_t start, now;
    const uint64_t ticks = (uint64_t)us * 24U;

    __asm volatile("isb; mrs %0, cntpct_el0" : "=r"(start));
    do {
        __asm volatile("isb; mrs %0, cntpct_el0" : "=r"(now));
    } while ((now - start) < ticks);
}

static bool wdt_is_running(void)
{
    return (REG32(WDT_WDTCNT) & WDTCNT_WDTEN) != 0U;
}

int rza3m_wdt_start(uint32_t period_ms)
{
    uint32_t wdttime;
    uint32_t set;

    if (wdt_is_running()) {
        return 1;                                   /* already running: cannot reconfigure */
    }

    /* WDTTIME = ceil(period / step) - 1, clamped to 0..0xFFF */
    wdttime = (uint32_t)((((uint64_t)period_ms * 1000U) + WDT_STEP_US - 1U) / WDT_STEP_US);
    wdttime = (wdttime == 0U) ? 0U : (wdttime - 1U);
    if (wdttime > WDTSET_WDTTIME_MAX) {
        wdttime = WDTSET_WDTTIME_MAX;
    }
    set = wdttime << WDTSET_WDTTIME_SHIFT;

    /* Overflow -> system reset (also makes the counter run with a debugger attached). */
    REG32(CPG_WDTRST_SEL) = WDTRSTSEL0 | WDTRSTSEL0_WEN;

    REG32(WDT_WDTSET) = set;
    wdt_delay_us(2U);                               /* reflection: 6 x P0 + 9 x OSCCLK */
    if (REG32(WDT_WDTSET) != set) {
        return -1;                                  /* not clocked / not released from reset */
    }

    REG32(WDT_WDTINT) = WDTINT_INTDISP;             /* clear any stale state */
    wdt_delay_us(2U);                               /* WDTINT write interval */
    REG32(WDT_WDTCNT) = WDTCNT_WDTEN;
    return wdt_is_running() ? 0 : -1;
}

bool rza3m_wdt_caused_last_reset(bool clear)
{
    bool wdt = (REG32(CPG_WDTOVF_RST) & WDTOVF0) != 0U;

    if (wdt && clear) {
        REG32(CPG_WDTOVF_RST) = WDTOVF0 | WDTOVF0_WEN;
    }
    return wdt;
}

