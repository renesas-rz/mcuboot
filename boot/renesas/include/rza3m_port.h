/* RZ/A3M MCUboot port - shared definitions (no FSP; platform code taken from the IPL) */
#ifndef RZA3M_PORT_H
#define RZA3M_PORT_H

#include <stdint.h>
#include <stddef.h>
#include "rza3m_flash_layout.h"

/* ---- Boot-info block, written by MCUboot at RZA3M_BOOTINFO_ADDR before the jump ---- */
#define RZA3M_BOOTINFO_MAGIC    0x4D434254UL   /* "MCBT" */
#define RZA3M_BOOTINFO_VERSION  1U
#define RZA3M_UI_BANK_NONE      0xFFFFFFFFUL
#define RZA3M_TLV_UI_BANK       0x00A0         /* imgtool --custom-tlv 0xA0 <decimal> */

typedef struct {
    uint32_t magic;
    uint32_t struct_version;
    uint32_t mode;
    uint32_t running_slot;     /* 0 = A, 1 = B */
    uint32_t image_flash_off;
    uint32_t ui_bank_id;
    uint8_t  ver_major;
    uint8_t  ver_minor;
    uint16_t ver_revision;
    uint32_t ver_build;
    uint32_t checksum;
} rza3m_boot_info_t;

static inline uint32_t rza3m_boot_info_sum(const rza3m_boot_info_t *bi)
{
    const uint32_t *w = (const uint32_t *)bi;
    uint32_t s = 0;
    for (size_t i = 0; i < offsetof(rza3m_boot_info_t, checksum) / 4U; i++) { s += w[i]; }
    return ~s;
}

/* ---- Platform layer (src/plat/rza3m_plat_ipl.c) ---- */
int  rza3m_plat_init(void);
void rza3m_plat_putc(char c);
void rza3m_plat_console_flush(void);

int  rza3m_nor_read(uint32_t off, void *dst, uint32_t len);
int  rza3m_nor_write(uint32_t off, const void *src, uint32_t len);
int  rza3m_nor_erase(uint32_t off, uint32_t len);

/* ---- Port services ---- */
void rza3m_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void rza3m_assert_fail(const char *file, int line) __attribute__((noreturn));

void rza3m_dcache_clean_inv_range(uintptr_t start, size_t len);
void rza3m_dcache_clean_inv_all(void);
void rza3m_disable_mmu_and_jump(uintptr_t entry) __attribute__((noreturn));

void rza3m_mcuboot_main(void) __attribute__((noreturn));   /* called from start.S */

#endif
