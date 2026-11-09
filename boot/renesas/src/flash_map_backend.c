/* RZ/A3M MCUboot port - flash map backend (shared by MCUboot and the application) */
#include "mcuboot_config/mcuboot_config.h"
#include "flash_map_backend/flash_map_backend.h"
#include "rza3m_port.h"

static const struct flash_area s_areas[] = {
    { .fa_id = FLASH_AREA_BOOTLOADER, .fa_device_id = FLASH_DEVICE_ID_NOR,
      .fa_off = RZA3M_PART_MCUBOOT_OFF, .fa_size = RZA3M_PART_MCUBOOT_SIZE },
    { .fa_id = FLASH_AREA_IMAGE_0,    .fa_device_id = FLASH_DEVICE_ID_NOR,
      .fa_off = RZA3M_PART_SLOT0_OFF,   .fa_size = RZA3M_PART_SLOT_SIZE },
    { .fa_id = FLASH_AREA_IMAGE_1,    .fa_device_id = FLASH_DEVICE_ID_NOR,
      .fa_off = RZA3M_PART_SLOT1_OFF,   .fa_size = RZA3M_PART_SLOT_SIZE },
#if defined(MCUBOOT_SWAP_USING_SCRATCH)
    { .fa_id = FLASH_AREA_IMAGE_SCRATCH, .fa_device_id = FLASH_DEVICE_ID_NOR,
      .fa_off = RZA3M_PART_SCRATCH_OFF, .fa_size = RZA3M_PART_SCRATCH_SIZE },
#endif
};
#define NUM_AREAS (sizeof(s_areas) / sizeof(s_areas[0]))

static const struct flash_area *find_area(uint8_t id)
{
    for (size_t i = 0; i < NUM_AREAS; i++) { if (s_areas[i].fa_id == id) { return &s_areas[i]; } }
    return NULL;
}

static int in_bounds(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    return (fa != NULL) && (off <= fa->fa_size) && (len <= fa->fa_size - off);
}

int flash_area_open(uint8_t id, const struct flash_area **area_outp)
{
    const struct flash_area *fa = find_area(id);
    if (fa == NULL || area_outp == NULL) { return -1; }
    *area_outp = fa;
    return 0;
}

void flash_area_close(const struct flash_area *fa) { (void)fa; }

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len)
{
    if (!in_bounds(fa, off, len)) { return -1; }
    return rza3m_nor_read(fa->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len)
{
    if (!in_bounds(fa, off, len)) { return -1; }
    return rza3m_nor_write(fa->fa_off + off, src, len);
}

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (!in_bounds(fa, off, len)) { return -1; }
    if ((off % RZA3M_NOR_ERASE_SIZE) != 0U || (len % RZA3M_NOR_ERASE_SIZE) != 0U) { return -1; }
    return rza3m_nor_erase(fa->fa_off + off, len);
}

uint32_t flash_area_align(const struct flash_area *fa)     { (void)fa; return RZA3M_NOR_WRITE_ALIGN; }
uint8_t  flash_area_erased_val(const struct flash_area *fa) { (void)fa; return RZA3M_NOR_ERASED_VAL; }

int flash_area_read_is_empty(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len)
{
    if (flash_area_read(fa, off, dst, len) != 0) { return -1; }
    const uint8_t *p = (const uint8_t *)dst;
    for (uint32_t i = 0; i < len; i++) { if (p[i] != RZA3M_NOR_ERASED_VAL) { return 0; } }
    return 1;
}

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors)
{
    const struct flash_area *fa = find_area((uint8_t)fa_id);
    if (fa == NULL || count == NULL || sectors == NULL) { return -1; }
    uint32_t n = fa->fa_size / RZA3M_NOR_ERASE_SIZE;
    if (n > *count) { return -1; }
    for (uint32_t i = 0; i < n; i++) {
        sectors[i].fs_off  = i * RZA3M_NOR_ERASE_SIZE;
        sectors[i].fs_size = RZA3M_NOR_ERASE_SIZE;
    }
    *count = n;
    return 0;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
{
    if (fa == NULL || sector == NULL || off >= fa->fa_size) { return -1; }
    sector->fs_off  = off - (off % RZA3M_NOR_ERASE_SIZE);
    sector->fs_size = RZA3M_NOR_ERASE_SIZE;
    return 0;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
#if defined(MCUBOOT_SWAP_USING_SCRATCH)
    case 2: return FLASH_AREA_IMAGE_SCRATCH;
#endif
    default: return -1;
    }
}

int flash_area_id_from_image_slot(int slot) { return flash_area_id_from_multi_image_slot(0, slot); }

int flash_area_id_from_image_offset(uint32_t offset)
{
    for (size_t i = 0; i < NUM_AREAS; i++) { if (s_areas[i].fa_off == offset) { return s_areas[i].fa_id; } }
    return -1;
}
