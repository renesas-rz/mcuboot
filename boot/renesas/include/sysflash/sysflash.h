#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__
#define FLASH_DEVICE_ID_NOR          0
#define FLASH_AREA_BOOTLOADER        0
#define FLASH_AREA_IMAGE_0           1   /* slot A / primary   */
#define FLASH_AREA_IMAGE_1           2   /* slot B / secondary */
#define FLASH_AREA_IMAGE_SCRATCH     3
#define FLASH_AREA_IMAGE_PRIMARY(x)   (((x) == 0) ? FLASH_AREA_IMAGE_0 : 255)
#define FLASH_AREA_IMAGE_SECONDARY(x) (((x) == 0) ? FLASH_AREA_IMAGE_1 : 255)
#endif
