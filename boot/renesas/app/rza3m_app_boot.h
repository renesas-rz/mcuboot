/*
 * RZ/A3M MCUboot port - application-side API.
 *
 * Add to the application build (same include paths and RZA3M_MCUBOOT_MODE as MCUboot):
 *   boot/rza3m/app/rza3m_app_boot.c
 *   boot/rza3m/src/flash_map_backend.c
 *   boot/bootutil/src/bootutil_public.c
 *   a NOR backend implementing rza3m_nor_read/write/erase()
 *
 * All functions access NOR: call them from one task, or serialise them with a mutex.
 */
#ifndef RZA3M_APP_BOOT_H
#define RZA3M_APP_BOOT_H
#include <stdbool.h>
#include <stdint.h>
#include "rza3m_port.h"

const rza3m_boot_info_t *rza3m_app_boot_info(void);
int      rza3m_app_is_confirmed(bool *confirmed);
int      rza3m_app_confirm(void);
int      rza3m_app_erase_inactive(void);
int      rza3m_app_write_inactive(uint32_t off, const void *data, uint32_t len);
int      rza3m_app_mark_pending(bool permanent);    /* then reset */
uint32_t rza3m_app_inactive_ui_bank(void);

#endif
