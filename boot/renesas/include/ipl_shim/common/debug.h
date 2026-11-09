/* Shim for TF-A <common/debug.h>: route vendored IPL driver logs to the MCUboot console.
 * ERROR/WARN always print; INFO/VERBOSE only with MCUBOOT_LOG_LEVEL >= DEBUG (make LOG=4). */
#ifndef IPL_SHIM_DEBUG_H
#define IPL_SHIM_DEBUG_H
#include "rza3m_port.h"

#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL 3
#endif

#define ERROR(...)   rza3m_log("[IPL-ERR] " __VA_ARGS__)
#define WARN(...)    rza3m_log("[IPL-WRN] " __VA_ARGS__)
#define NOTICE(...)  rza3m_log(__VA_ARGS__)
#if (MCUBOOT_LOG_LEVEL >= 4)
#define INFO(...)    rza3m_log("[IPL-INF] " __VA_ARGS__)
#define VERBOSE(...) rza3m_log("[IPL-VER] " __VA_ARGS__)
#else
#define INFO(...)    do { } while (0)
#define VERBOSE(...) do { } while (0)
#endif
#define panic()      rza3m_assert_fail(__FILE__, __LINE__)
#endif
