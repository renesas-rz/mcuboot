/* RZ/A3M MCUboot port - watchdog (WDT CH0), shared by MCUboot and the application. */
#ifndef RZA3M_WDT_H
#define RZA3M_WDT_H
#include <stdbool.h>
#include <stdint.h>

#ifndef RZA3M_WDT_PERIOD_MS
#define RZA3M_WDT_PERIOD_MS      5000U    /* reset ~10 s after jump if never refreshed */
#endif

/* Start WDT CH0. Returns 0 ok, 1 already running (unchanged), -1 on failure. */
int  rza3m_wdt_start(uint32_t period_ms);

/* true if the last reset was a WDT system reset; optionally clear the flag. */
bool rza3m_wdt_caused_last_reset(bool clear);

#endif

