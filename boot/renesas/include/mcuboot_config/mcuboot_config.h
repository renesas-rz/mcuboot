/* RZ/A3M MCUboot port - build configuration. Mode: make MODE=<n> (same for MCUboot and app). */
#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

#include "rza3m_flash_layout.h"

#define RZA3M_MODE_RAM_LOAD_REVERT   1
#define RZA3M_MODE_SWAP_MOVE         2
#define RZA3M_MODE_SWAP_SCRATCH      3
#define RZA3M_MODE_OVERWRITE_ONLY    4

#ifndef RZA3M_MCUBOOT_MODE
#define RZA3M_MCUBOOT_MODE RZA3M_MODE_RAM_LOAD_REVERT
#endif

#if   RZA3M_MCUBOOT_MODE == RZA3M_MODE_RAM_LOAD_REVERT
#  define MCUBOOT_RAM_LOAD              1
#  define MCUBOOT_RAM_LOAD_REVERT       1   /* verify it exists in the pinned tag */
#  define IMAGE_EXECUTABLE_RAM_START    RZA3M_APP_LOAD_ADDR
#  define IMAGE_EXECUTABLE_RAM_SIZE     RZA3M_APP_RAM_SIZE
#  define RZA3M_MODE_NAME               "ram-load+revert"
#elif RZA3M_MCUBOOT_MODE == RZA3M_MODE_SWAP_MOVE
#  define MCUBOOT_SWAP_USING_MOVE       1
#  define MCUBOOT_VALIDATE_PRIMARY_SLOT 1
#  define RZA3M_MODE_NAME               "swap-move"
#elif RZA3M_MCUBOOT_MODE == RZA3M_MODE_SWAP_SCRATCH
#  define MCUBOOT_SWAP_USING_SCRATCH    1
#  define MCUBOOT_VALIDATE_PRIMARY_SLOT 1
#  define RZA3M_MODE_NAME               "swap-scratch"
#elif RZA3M_MCUBOOT_MODE == RZA3M_MODE_OVERWRITE_ONLY
#  define MCUBOOT_OVERWRITE_ONLY        1
#  define MCUBOOT_VALIDATE_PRIMARY_SLOT 1
#  define RZA3M_MODE_NAME               "overwrite-only"
#else
#  error "Unknown RZA3M_MCUBOOT_MODE"
#endif

#define MCUBOOT_IMAGE_NUMBER            1
/* Phase 1: SHA-256 integrity only (no MCUBOOT_SIGN_*). Phase 2: MCUBOOT_SIGN_EC256 + keys. */
#define MCUBOOT_USE_TINYCRYPT           1

#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS 1
#define MCUBOOT_MAX_IMG_SECTORS         (RZA3M_PART_SLOT_SIZE / RZA3M_NOR_ERASE_SIZE)
#define MCUBOOT_BOOT_MAX_ALIGN          8

#define MCUBOOT_LOG_LEVEL_OFF      0
#define MCUBOOT_LOG_LEVEL_ERROR    1
#define MCUBOOT_LOG_LEVEL_WARNING  2
#define MCUBOOT_LOG_LEVEL_INFO     3
#define MCUBOOT_LOG_LEVEL_DEBUG    4
#define MCUBOOT_HAVE_LOGGING       1
#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL          MCUBOOT_LOG_LEVEL_INFO
#endif
#define MCUBOOT_HAVE_ASSERT_H      1

#define MCUBOOT_WATCHDOG_FEED()    do { } while (0)   /* kick here if the IPL leaves a WDT running */
#define MCUBOOT_CPU_IDLE()         do { } while (0)

#endif
