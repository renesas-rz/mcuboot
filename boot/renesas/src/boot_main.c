/*
 * RZ/A3M MCUboot port - boot application.
 *
 * Entered from start.S after the stock IPL has copied this image into DDR:
 *   boot_go() -> (RAM-load: image already in DDR | swap/overwrite: copy primary slot to DDR)
 *             -> boot-info block -> clean caches, MMU off -> jump to the application
 */
#include <string.h>
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include "rza3m_port.h"

BOOT_LOG_MODULE_REGISTER(rza3m);

static void rza3m_halt(const char *why) __attribute__((noreturn));
static void rza3m_halt(const char *why)
{
    BOOT_LOG_ERR("HALT: %s", why);
    rza3m_plat_console_flush();
    for (;;) {
        __asm volatile("wfe");
    }
}

/* UI bank number from the custom TLV 0xA0 (ASCII decimal), or RZA3M_UI_BANK_NONE. */
static uint32_t parse_ui_bank(const struct image_header *hdr, const struct flash_area *fap)
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    uint16_t type;
    char txt[12];
    uint32_t v = 0U;

    if (bootutil_tlv_iter_begin(&it, hdr, fap, RZA3M_TLV_UI_BANK, true) != 0) { return RZA3M_UI_BANK_NONE; }
    if (bootutil_tlv_iter_next(&it, &off, &len, &type) != 0)                  { return RZA3M_UI_BANK_NONE; }
    if ((len == 0U) || (len >= sizeof(txt)))                                   { return RZA3M_UI_BANK_NONE; }
    if (flash_area_read(fap, off, txt, len) != 0)                              { return RZA3M_UI_BANK_NONE; }

    for (uint16_t i = 0U; i < len; i++) {
        if ((txt[i] < '0') || (txt[i] > '9')) { return RZA3M_UI_BANK_NONE; }
        v = (v * 10U) + (uint32_t)(txt[i] - '0');
    }
    return v;
}

void rza3m_mcuboot_main(void)
{
    struct boot_rsp rsp;
    uintptr_t load;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    if (rza3m_plat_init() != 0) {
        rza3m_halt("platform init failed");
    }

    BOOT_LOG_INF("RZ/A3M MCUboot - mode %s", RZA3M_MODE_NAME);

    FIH_CALL(boot_go, fih_rc, &rsp);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        rza3m_halt("no bootable image");
    }

    const struct image_header *hdr = rsp.br_hdr;

#if defined(MCUBOOT_RAM_LOAD)
    load = (uintptr_t)hdr->ih_load_addr;          /* copied and validated by bootutil */
#else
    {
        const struct flash_area *pfa;
        uint32_t total = (uint32_t)hdr->ih_hdr_size + hdr->ih_img_size;

        if (total > RZA3M_APP_RAM_SIZE) {
            rza3m_halt("image larger than the DDR application region");
        }
        if ((flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &pfa) != 0) ||
            (flash_area_read(pfa, 0, (void *)(uintptr_t)RZA3M_APP_LOAD_ADDR, total) != 0)) {
            rza3m_halt("copy of the primary slot to DDR failed");
        }
        flash_area_close(pfa);
        load = RZA3M_APP_LOAD_ADDR;
    }
#endif

    uintptr_t entry = load + hdr->ih_hdr_size;
    if (entry != RZA3M_APP_ENTRY_ADDR) {
        BOOT_LOG_WRN("entry 0x%lx != expected 0x%lx",
                     (unsigned long)entry, (unsigned long)RZA3M_APP_ENTRY_ADDR);
    }

    rza3m_boot_info_t *bi = (rza3m_boot_info_t *)(uintptr_t)RZA3M_BOOTINFO_ADDR;
    memset(bi, 0, sizeof(*bi));
    bi->magic           = RZA3M_BOOTINFO_MAGIC;
    bi->struct_version  = RZA3M_BOOTINFO_VERSION;
    bi->mode            = RZA3M_MCUBOOT_MODE;
    bi->image_flash_off = rsp.br_image_off;
    bi->running_slot    = (rsp.br_image_off == RZA3M_PART_SLOT1_OFF) ? 1U : 0U;
    bi->ver_major       = hdr->ih_ver.iv_major;
    bi->ver_minor       = hdr->ih_ver.iv_minor;
    bi->ver_revision    = hdr->ih_ver.iv_revision;
    bi->ver_build       = hdr->ih_ver.iv_build_num;
    bi->ui_bank_id      = RZA3M_UI_BANK_NONE;

    {
        const struct flash_area *rfa;
        int area_id = flash_area_id_from_image_offset(rsp.br_image_off);

        if ((area_id >= 0) && (flash_area_open((uint8_t)area_id, &rfa) == 0)) {
            bi->ui_bank_id = parse_ui_bank(hdr, rfa);
            flash_area_close(rfa);
        }
    }
    bi->checksum = rza3m_boot_info_sum(bi);

    BOOT_LOG_INF("boot slot %c  v%u.%u.%u+%lu  ui_bank %ld  entry 0x%lx",
                 bi->running_slot ? 'B' : 'A', bi->ver_major, bi->ver_minor, bi->ver_revision,
                 (unsigned long)bi->ver_build, (long)(int32_t)bi->ui_bank_id, (unsigned long)entry);

    rza3m_plat_console_flush();
    rza3m_disable_mmu_and_jump(entry);           /* does not return */
}

/* Called from the vector table in start.S. */
void rza3m_exception_report(uint64_t esr, uint64_t elr, uint64_t far,
                            uint64_t spsr, uint64_t vec, uint64_t sp, uint64_t lr)
{
    BOOT_LOG_ERR("exception: vec=%lu EC=0x%lx ISS=0x%lx",
                 (unsigned long)vec, (unsigned long)((esr >> 26) & 0x3FU),
                 (unsigned long)(esr & 0x1FFFFFFU));
    BOOT_LOG_ERR("exception: ELR=0x%lx FAR=0x%lx", (unsigned long)elr, (unsigned long)far);
    BOOT_LOG_ERR("exception: LR=0x%lx SP=0x%lx SPSR=0x%lx",
                 (unsigned long)lr, (unsigned long)sp, (unsigned long)spsr);
    rza3m_plat_console_flush();
}
