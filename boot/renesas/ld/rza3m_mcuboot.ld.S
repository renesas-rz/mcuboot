/*
 * RZ/A3M MCUboot - linker script WITH the IPL application header.
 * Preprocessed by the Makefile: cpp -P -DLINKER_SCRIPT
 *
 * Header format: RZ/A3 IPL application note R01AN6413EJ0120, 5.7 "Application Header"
 *   +0x000 QWORD  entry point            +0x008 QWORD ~entry point
 *   +0x010 QWORD  payload copy address   +0x018 QWORD ~copy address
 *   +0x020 QWORD  payload size           +0x028 QWORD ~payload size
 *   +0x030        reserved (0)
 *   +0x1E0        "RZ FSP Application", zero padded (32 bytes)
 *   +0x200        payload  -> copied by the IPL to the copy address, then jump to entry
 *
 * The header lives only in NOR (LMA); code/data are linked in DDR (VMA) and stored
 * right after the header (AT> NOR_IMG). objcopy -O srec/binary uses the LMAs, so the
 * output is: header @ FSP_BASE, payload @ FSP_BASE + 0x200.
 */
#include "rza3m_flash_layout.h"

#define RZA3M_FSP_BASE       (RZA3M_NOR_MMAP_BASE + RZA3M_PART_MCUBOOT_OFF)  /* = IPL make option FSP_BASE */
#define RZA3M_APPHDR_SIZE    0x200

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY
{
    NOR_HDR (r)   : ORIGIN = RZA3M_FSP_BASE,                     LENGTH = RZA3M_APPHDR_SIZE
    NOR_IMG (r)   : ORIGIN = RZA3M_FSP_BASE + RZA3M_APPHDR_SIZE, LENGTH = RZA3M_PART_MCUBOOT_SIZE - RZA3M_APPHDR_SIZE
    DDR     (rwx) : ORIGIN = RZA3M_MCUBOOT_DDR_BASE,              LENGTH = RZA3M_MCUBOOT_DDR_SIZE
}

SECTIONS
{
    .app_header : {
        QUAD(ABSOLUTE(_start))
        QUAD(~ABSOLUTE(_start))
        QUAD(ABSOLUTE(__payload_dest))
        QUAD(~ABSOLUTE(__payload_dest))
        QUAD(__payload_size)
        QUAD(~ABSOLUTE(__payload_size))
        FILL(0x00)
        . = 0x1E0;
        BYTE(0x52) BYTE(0x5A) BYTE(0x20) BYTE(0x46) BYTE(0x53) BYTE(0x50) BYTE(0x20)  /* "RZ FSP " */
        BYTE(0x41) BYTE(0x70) BYTE(0x70) BYTE(0x6C) BYTE(0x69) BYTE(0x63) BYTE(0x61)  /* "Applica" */
        BYTE(0x74) BYTE(0x69) BYTE(0x6F) BYTE(0x6E)                                   /* "tion"    */
        . = RZA3M_APPHDR_SIZE;
    } > NOR_HDR

    .text : ALIGN(8) {
        __payload_dest = .;
        KEEP(*(.text.entry))
        *(.text*)
    } > DDR AT> NOR_IMG

    .rodata : ALIGN(8) { *(.rodata*) } > DDR AT> NOR_IMG

    .data : ALIGN(8) {
        *(.data*)
        . = ALIGN(8);
    } > DDR AT> NOR_IMG

    __image_end    = .;
    __payload_size = __image_end - __payload_dest;
    __load_base    = __payload_dest;

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss*) *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } > DDR

    .stack (NOLOAD) : ALIGN(16) {
        . += 0x10000;
        __stack_top = .;
    } > DDR

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

ASSERT(__payload_dest == RZA3M_MCUBOOT_DDR_BASE, "payload must start at the DDR base")
ASSERT(__payload_size + RZA3M_APPHDR_SIZE <= RZA3M_PART_MCUBOOT_SIZE, "MCUboot larger than its NOR partition")
ASSERT(__stack_top <= RZA3M_APP_LOAD_ADDR,
       "MCUboot (incl. stack) overlaps the application RAM-load window")
