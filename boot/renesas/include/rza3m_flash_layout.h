/*
 * RZ/A3M MCUboot port - NOR partition map and DDR memory contract.
 * Included by C, assembly and the linker script (preprocessed with -DLINKER_SCRIPT).
 *
 * Every TODO must be checked against the stock IPL (rza-initial-program-loader):
 *   plat/renesas/rz/...           : NOR source offset, DDR destination, copy size, jump address
 *   application_note/04.03.x      : EK-RZA3M memory maps
 * Then build with LAYOUT_OK=1 (defines RZA3M_LAYOUT_CONFIRMED).
 */
#ifndef RZA3M_FLASH_LAYOUT_H
#define RZA3M_FLASH_LAYOUT_H

#if defined(LINKER_SCRIPT) || defined(__ASSEMBLER__)
#define RZA3M_U(x) x
#else
#define RZA3M_U(x) x##UL
#endif

/* ---------------- What the stock IPL does with "the application" ---------------- */
#define RZA3M_MCUBOOT_DDR_BASE     RZA3M_U(0x40001000)  /* TODO: DDR address the IPL copies to AND jumps to */
#define RZA3M_MCUBOOT_DDR_SIZE     RZA3M_U(0x001F0000)  /* TODO: <2 MB for code+data+bss+stack              */

/* ---------------- NOR device ---------------- */
#define RZA3M_NOR_MMAP_BASE        RZA3M_U(0x20000000)  /* TODO: xSPI memory-mapped window                  */
#define RZA3M_NOR_SIZE             RZA3M_U(0x02000000)  /* TODO: fitted device (32 MB -> 4-byte addressing) */
#define RZA3M_NOR_ERASE_SIZE       RZA3M_U(0x00010000)  /* 64 KB block erase = MCUboot "sector"             */
#define RZA3M_NOR_PAGE_SIZE        RZA3M_U(256)
#define RZA3M_NOR_WRITE_ALIGN      8U                   /* imgtool --align must match                      */
#define RZA3M_NOR_ERASED_VAL       0xFFU

/* ---------------- NOR partitions (offset from NOR start) ---------------- */
/* 0 .. : IPL, untouched */
#define RZA3M_PART_MCUBOOT_OFF     RZA3M_U(0x00020000)  /* TODO: = NOR offset the IPL loads the app from */
#define RZA3M_PART_MCUBOOT_SIZE    RZA3M_U(0x00080000)  /* <= IPL copy size                             */
#define RZA3M_PART_SLOT0_OFF       RZA3M_U(0x00100000)  /* Application A */
#define RZA3M_PART_SLOT1_OFF       RZA3M_U(0x00200000)  /* Application B */
#define RZA3M_PART_SLOT_SIZE       RZA3M_U(0x00100000)
#define RZA3M_PART_SCRATCH_OFF     RZA3M_U(0x00E00000)  /* swap-scratch mode only */
#define RZA3M_PART_SCRATCH_SIZE    RZA3M_U(0x00040000)

/* ---------------- DDR: FreeRTOS application ---------------- */
#define RZA3M_APP_LOAD_ADDR        RZA3M_U(0x401FF000)  /* image HEADER lands here                  */
#define RZA3M_APP_RAM_SIZE         RZA3M_U(0x00600000)
#define RZA3M_IMG_HDR_SIZE         RZA3M_U(0x1000)      /* imgtool --header-size                    */
#define RZA3M_APP_ENTRY_ADDR       (RZA3M_APP_LOAD_ADDR + RZA3M_IMG_HDR_SIZE)  /* link the app here */
#define RZA3M_BOOTINFO_ADDR        RZA3M_U(0x407FF000)  /* 4 KB, outside MCUboot and app regions    */

#if !defined(LINKER_SCRIPT) && !defined(__ASSEMBLER__)
#ifndef RZA3M_LAYOUT_CONFIRMED
#warning "Verify rza3m_flash_layout.h against the IPL memory map, then build with LAYOUT_OK=1"
#endif
#if (RZA3M_PART_SLOT_SIZE % RZA3M_NOR_ERASE_SIZE) != 0
#error "Slot size must be a multiple of the erase size"
#endif
#if (RZA3M_PART_SLOT1_OFF < RZA3M_PART_SLOT0_OFF + RZA3M_PART_SLOT_SIZE)
#error "Slot 0 and slot 1 overlap"
#endif
#if (RZA3M_PART_SCRATCH_OFF + RZA3M_PART_SCRATCH_SIZE) > RZA3M_NOR_SIZE
#error "Partition map exceeds NOR size"
#endif
#if (RZA3M_APP_LOAD_ADDR < RZA3M_MCUBOOT_DDR_BASE + RZA3M_MCUBOOT_DDR_SIZE) && \
    (RZA3M_MCUBOOT_DDR_BASE < RZA3M_APP_LOAD_ADDR + RZA3M_APP_RAM_SIZE)
#error "MCUboot DDR region overlaps the application region"
#endif
#endif

#endif
