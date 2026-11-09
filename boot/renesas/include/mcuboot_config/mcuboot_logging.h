#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__
#include "mcuboot_config/mcuboot_config.h"
#include "rza3m_port.h"

#define MCUBOOT_LOG_MODULE_DECLARE(domain)
#define MCUBOOT_LOG_MODULE_REGISTER(domain)
#define RZA3M_LOG_IF(lvl, tag, fmt, ...) \
    do { if (MCUBOOT_LOG_LEVEL >= (lvl)) rza3m_log("[" tag "] " fmt "\n", ##__VA_ARGS__); } while (0)
#define MCUBOOT_LOG_ERR(fmt, ...) RZA3M_LOG_IF(MCUBOOT_LOG_LEVEL_ERROR,   "ERR", fmt, ##__VA_ARGS__)
#define MCUBOOT_LOG_WRN(fmt, ...) RZA3M_LOG_IF(MCUBOOT_LOG_LEVEL_WARNING, "WRN", fmt, ##__VA_ARGS__)
#define MCUBOOT_LOG_INF(fmt, ...) RZA3M_LOG_IF(MCUBOOT_LOG_LEVEL_INFO,    "INF", fmt, ##__VA_ARGS__)
#define MCUBOOT_LOG_DBG(fmt, ...) RZA3M_LOG_IF(MCUBOOT_LOG_LEVEL_DEBUG,   "DBG", fmt, ##__VA_ARGS__)
#define MCUBOOT_LOG_SIM(...)      do { } while (0)
#endif
