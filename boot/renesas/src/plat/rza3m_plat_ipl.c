/*
 * RZ/A3M MCUboot port - platform layer.
 *
 * Console : SCIFA0, already configured by the IPL (TX only, polled).
 * NOR     : read through the memory-mapped xSPI window.
 *           Write/erase through the vendored IPL v2.3.1 drivers
 *           (spim + qspiflash_mx25l25645g) - EXPERIMENTAL, see README.
 *
 * Build options:
 *   RZA3M_NOR_DRIVER=0  read-only NOR, write/erase return -1
 *   RZA3M_NOR_DRIVER=1  (default) NOR write/erase enabled
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <lib/mmio.h>
#include "rza3m_port.h"

#ifndef RZA3M_NOR_DRIVER
#define RZA3M_NOR_DRIVER 1
#endif

/* ------------------------------------------------------------------ console */
#define SCIF0_BASE   0x1004B800UL
#define SCIF_SCR     0x04U
#define SCIF_FTDR    0x06U
#define SCIF_FSR     0x08U
#define SCR_TE       0x20U
#define FSR_TDFE     (1U << 5)
#define FSR_TEND     (1U << 6)
#define SCIF_TMO     2000000UL
#define SCIF_R8(o)   (*(volatile uint8_t  *)(uintptr_t)(SCIF0_BASE + (o)))
#define SCIF_R16(o)  (*(volatile uint16_t *)(uintptr_t)(SCIF0_BASE + (o)))

void rza3m_plat_putc(char c)
{
    uint32_t t = SCIF_TMO;
    while (((SCIF_R16(SCIF_FSR) & FSR_TDFE) == 0U) && (--t != 0U)) { }
    SCIF_R8(SCIF_FTDR) = (uint8_t)c;
    SCIF_R16(SCIF_FSR) = (uint16_t)(SCIF_R16(SCIF_FSR) & (uint16_t)~(FSR_TDFE | FSR_TEND));
}

void rza3m_plat_console_flush(void)
{
    uint32_t t = SCIF_TMO;
    while (((SCIF_R16(SCIF_FSR) & FSR_TEND) == 0U) && (--t != 0U)) { }
}

static void console_init(void)
{
    uint16_t scr = SCIF_R16(SCIF_SCR);
    if ((scr & SCR_TE) == 0U) {
        SCIF_R16(SCIF_SCR) = (uint16_t)(scr | SCR_TE);
    }
}

/* ------------------------------------------------------------------ NOR read (both builds) */
static int nor_range_ok(uint32_t off, uint32_t len)
{
    return (off <= RZA3M_NOR_SIZE) && (len <= RZA3M_NOR_SIZE - off);
}

#if !RZA3M_NOR_DRIVER

int rza3m_plat_init(void)
{
    console_init();
    return 0;
}

int rza3m_nor_read(uint32_t off, void *dst, uint32_t len)
{
    if (!nor_range_ok(off, len)) { return -1; }
    memcpy(dst, (const void *)(uintptr_t)(RZA3M_NOR_MMAP_BASE + off), len);
    return 0;
}

int rza3m_nor_write(uint32_t off, const void *src, uint32_t len) { (void)off; (void)src; (void)len; return -1; }
int rza3m_nor_erase(uint32_t off, uint32_t len)                  { (void)off; (void)len; return -1; }

#else /* RZA3M_NOR_DRIVER */

#include "xspi_api.h"
#include "xspidevice_api.h"
#include "spim_regs.h"

#define RZA3M_SPIM_BASE  0x10060000UL
#define REG(o)           (RZA3M_SPIM_BASE + (o))
#define NOR_TMO          10000000U
#define DEV              (xspidevices[0])

static bool s_nor_ready;   /* driver opened and memory-mapped mode configured */
static bool s_nor_manual;  /* controller currently in manual (command) mode  */

static void nor_flush_read_cache(uint32_t off, uint32_t len)
{
    mmio_setbits_32(REG(SPIM_DRCR), SPIM_DRCR_RCF);
    (void)mmio_read_32(REG(SPIM_DRCR));               /* dummy read required after RCF */
    rza3m_dcache_clean_inv_range(RZA3M_NOR_MMAP_BASE + off, len);
}

/* Negate CS left asserted by memory-mapped reads (controller must be in mmap mode). */
static int nor_release_cs(void)
{
    uint32_t n;

    if ((mmio_read_32(REG(SPIM_CMNCR)) & SPIM_CMNCR_MD) != 0U) {
        mmio_clrbits_32(REG(SPIM_CMNCR), SPIM_CMNCR_MD);
    }
    mmio_clrbits_32(REG(SPIM_DRCR), SPIM_DRCR_SSLE);
    mmio_setbits_32(REG(SPIM_DRCR), SPIM_DRCR_SSLN);
    for (n = NOR_TMO; ((mmio_read_32(REG(SPIM_CMNSR)) & SPIM_CMNSR_SSLF) != 0U) && (--n != 0U); ) { }
    return (n == 0U) ? -1 : 0;
}

/* One-time driver open; leaves the controller in memory-mapped mode. */
static int nor_attach(void)
{
    if (s_nor_ready) { return 0; }

    if (nor_release_cs() != 0)                    { return -1; }
    if (DEV->api->open(DEV->ctrl, DEV->cfg) != 0) { return -1; }
    if (DEV->api->enter_xip(DEV->ctrl) != 0)      { return -1; }

    nor_flush_read_cache(0U, 0U);
    s_nor_ready  = true;
    s_nor_manual = false;
    return 0;
}

/* Memory-mapped -> manual command mode. */
static int nor_begin(void)
{
    if (nor_attach() != 0) { return -1; }
    if (s_nor_manual)      { return 0; }

    if (nor_release_cs() != 0)               { return -1; }
    if (DEV->api->exit_xip(DEV->ctrl) != 0)  { return -1; }

    s_nor_manual = true;
    return 0;
}

/* Manual command mode -> memory-mapped. Safe to call when already mapped. */
static int nor_end(void)
{
    if (!s_nor_ready || !s_nor_manual) { return 0; }

    if (DEV->api->enter_xip(DEV->ctrl) != 0) { return -1; }

    __asm volatile("dsb sy\n\tisb" ::: "memory");
    s_nor_manual = false;
    return 0;
}

int rza3m_plat_init(void)
{
    console_init();
    if ((xspidevices_count < 1) || (DEV == NULL)) {
        rza3m_log("[ERR] no xSPI device\n");
        return -1;
    }
    return 0;
}

int rza3m_nor_read(uint32_t off, void *dst, uint32_t len)
{
    if (!nor_range_ok(off, len)) { return -1; }
    if (nor_end() != 0)          { return -1; }   /* reads require memory-mapped mode */
    memcpy(dst, (const void *)(uintptr_t)(RZA3M_NOR_MMAP_BASE + off), len);
    return 0;
}

int rza3m_nor_write(uint32_t off, const void *src, uint32_t len)
{
    int rc;

    if ((src == NULL) && (len != 0U)) { return -1; }
    if (!nor_range_ok(off, len))      { return -1; }
    if (len == 0U)                    { return 0; }

    rc = nor_begin();
    if (rc == 0) {
        rc = DEV->api->write(DEV->ctrl, src, (size_t)off, (size_t)len);
    }
    if (nor_end() != 0) { rc = -1; }
    nor_flush_read_cache(off, len);

    return (rc == 0) ? 0 : -1;
}

int rza3m_nor_erase(uint32_t off, uint32_t len)
{
    int rc;

    if (((off % RZA3M_NOR_ERASE_SIZE) != 0U) || ((len % RZA3M_NOR_ERASE_SIZE) != 0U)) { return -1; }
    if (!nor_range_ok(off, len)) { return -1; }
    if (len == 0U)               { return 0; }

    rc = nor_begin();
    if (rc == 0) {
        rc = DEV->api->erase(DEV->ctrl, (size_t)off, (size_t)len);
    }
    if (nor_end() != 0) { rc = -1; }
    nor_flush_read_cache(off, len);

    return (rc == 0) ? 0 : -1;
}

#endif /* RZA3M_NOR_DRIVER */
