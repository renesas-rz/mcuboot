/* Minimal platform_def.h for the vendored IPL drivers.
 * Copy ONLY the macros the files in src/plat/ipl/ use (the compiler lists them as
 * "undeclared"), with values from the IPL's plat/renesas/rz/.../platform_def.h + rz_soc_def.h.
 * Example (VERIFY name and value in IPL v2.3.1):
 *   #define RZ_SPIBSC_BASE   0x...
 */
#ifndef IPL_SHIM_PLATFORM_DEF_H
#define IPL_SHIM_PLATFORM_DEF_H
#include <lib/utils_def.h>

#define RZ_XSPI_EXCLUSIVE_SELECTOR 	1
#define RZ_XSPI_FORCE_VOLTAGE_SETTING 	0
#define RZ_SPIM_SDR_FREQ_LIMIT 		66625000

#endif
