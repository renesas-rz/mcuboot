#ifndef __MCUBOOT_ASSERT_H__
#define __MCUBOOT_ASSERT_H__
#include "rza3m_port.h"
#undef assert
#define assert(x) do { if (!(x)) rza3m_assert_fail(__FILE__, __LINE__); } while (0)
#endif
