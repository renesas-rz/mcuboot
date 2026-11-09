/* Shim for <assert.h> (no libc): same behaviour as MCUboot's mcuboot_assert.h */
#ifndef IPL_SHIM_ASSERT_H
#define IPL_SHIM_ASSERT_H
#include "rza3m_port.h"
#undef assert
#define assert(x) do { if (!(x)) { rza3m_assert_fail(__FILE__, __LINE__); } } while (0)
#endif
