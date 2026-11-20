/* RZ/A3M MCUboot port - application-side API (see rza3m_app_boot.h). */
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_public.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include "rza3m_app_boot.h"

const rza3m_boot_info_t *rza3m_app_boot_info(void)
{
    const rza3m_boot_info_t *bi = (const rza3m_boot_info_t *)(uintptr_t)RZA3M_BOOTINFO_ADDR;

    if ((bi->magic != RZA3M_BOOTINFO_MAGIC) || (bi->struct_version != RZA3M_BOOTINFO_VERSION) ||
        (bi->checksum != rza3m_boot_info_sum(bi))) {
        return NULL;
    }
    return bi;
}

static uint8_t running_area_id(void)
{
#if defined(MCUBOOT_RAM_LOAD)
    const rza3m_boot_info_t *bi = rza3m_app_boot_info();
    return ((bi != NULL) && (bi->running_slot == 1U)) ? FLASH_AREA_IMAGE_SECONDARY(0)
                                                       : FLASH_AREA_IMAGE_PRIMARY(0);
#else
    return FLASH_AREA_IMAGE_PRIMARY(0);
#endif
}

static uint8_t inactive_area_id(void)
{
#if defined(MCUBOOT_RAM_LOAD)
    return (running_area_id() == FLASH_AREA_IMAGE_PRIMARY(0)) ? FLASH_AREA_IMAGE_SECONDARY(0)
                                                               : FLASH_AREA_IMAGE_PRIMARY(0);
#else
    return FLASH_AREA_IMAGE_SECONDARY(0);
#endif
}

int rza3m_app_is_confirmed(bool *confirmed)
{
    const struct flash_area *fa;
    struct boot_swap_state st;
    int rc;

    if (confirmed == NULL) { return -1; }
    if (flash_area_open(running_area_id(), &fa) != 0) { return -1; }
    rc = boot_read_swap_state(fa, &st);
    flash_area_close(fa);
    if (rc == 0) { *confirmed = (st.image_ok == BOOT_FLAG_SET); }
    return rc;
}

int rza3m_app_confirm(void)
{
#if defined(MCUBOOT_RAM_LOAD)
    const struct flash_area *fa;
    int rc;

    if (flash_area_open(running_area_id(), &fa) != 0) { return -1; }
    rc = boot_set_next(fa, true, true);
    flash_area_close(fa);
    return rc;
#else
    return boot_set_confirmed();
#endif
}

int rza3m_app_erase_inactive(void)
{
    const struct flash_area *fa;
    int rc;

    if (flash_area_open(inactive_area_id(), &fa) != 0) { return -1; }
    rc = flash_area_erase(fa, 0, flash_area_get_size(fa));
    flash_area_close(fa);
    return rc;
}

int rza3m_app_write_inactive(uint32_t off, const void *data, uint32_t len)
{
    const struct flash_area *fa;
    int rc;

    if (flash_area_open(inactive_area_id(), &fa) != 0) { return -1; }
    rc = flash_area_write(fa, off, data, len);
    flash_area_close(fa);
    return rc;
}

int rza3m_app_mark_pending(bool permanent)
{
#if defined(MCUBOOT_RAM_LOAD)
    const struct flash_area *fa;
    int rc;

    if (flash_area_open(inactive_area_id(), &fa) != 0) { return -1; }
    rc = boot_set_next(fa, false, permanent);
    flash_area_close(fa);
    return rc;
#else
    return boot_set_pending(permanent ? 1 : 0);
#endif
}
