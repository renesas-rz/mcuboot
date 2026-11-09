#include "hal_data.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <stdint.h>
int rza3m_nor_read(uint32_t offset,
                void * destination,
                 uint32_t length);

int rza3m_nor_write(uint32_t offset,
                  const void * source,
                  uint32_t length);

int rza3m_nor_erase(uint32_t offset, uint32_t length);

#define APP_NOR_MMAP_BASE       0x20000000UL
#define APP_NOR_SIZE            0x02000000UL
#define APP_NOR_PAGE_SIZE       256U
#define APP_NOR_ERASE_SIZE      0x10000U

static bool g_nor_opened;

static int rza3m_nor_open_once(void)
{
    if (g_nor_opened)
    {
        return 0;
    }

    fsp_err_t err = g_spi_flash0.p_api->open(
        g_spi_flash0.p_ctrl,
        g_spi_flash0.p_cfg);

    if ((FSP_SUCCESS != err) &&
        (FSP_ERR_ALREADY_OPEN != err))
    {
        return -(int) err;
    }

    g_nor_opened = true;

    return 0;
}

static int rza3m_nor_begin(void)
{
    return rza3m_nor_open_once();
}

static int rza3m_nor_end(void)
{
    fsp_err_t err ;
    /*
     * Ensure that the XIP-mode change completes before any subsequent
     * memory-mapped NOR access.
     */
    __asm__ volatile ("dsb sy\nisb" ::: "memory");

    return (FSP_SUCCESS == err) ? 0 : -(int) err;
}

static int rza3m_nor_wait_ready(void)
{
    spi_flash_status_t status;

    do
    {
        fsp_err_t err = g_spi_flash0.p_api->statusGet(
            g_spi_flash0.p_ctrl,
            &status);

        if (FSP_SUCCESS != err)
        {
            return -(int) err;
        }
    }
    while (status.write_in_progress);

    return 0;
}

int rza3m_nor_read(uint32_t offset, void * destination, uint32_t length)
{
    if ((destination == NULL) && (length != 0U))
    {
        return -1;
    }

    if ((offset > APP_NOR_SIZE) ||
        (length > (APP_NOR_SIZE - offset)))
    {
        return -1;
    }

    memcpy(
        destination,
        (const void *)(uintptr_t)(APP_NOR_MMAP_BASE + offset),
        length);

    return 0;
}

int rza3m_nor_erase(uint32_t offset, uint32_t length)
{
    if (((offset & (APP_NOR_ERASE_SIZE - 1U)) != 0U) ||
        ((length & (APP_NOR_ERASE_SIZE - 1U)) != 0U))
    {
        return -1;
    }

    if ((offset > APP_NOR_SIZE) ||
        (length > (APP_NOR_SIZE - offset)))
    {
        return -1;
    }

    if (length == 0U)
    {
        return 0;
    }

    int rc = rza3m_nor_begin();

    if (rc != 0)
    {
        return rc;
    }

    uint32_t address = offset;
    uint32_t remaining = length;

    while (remaining > 0U)
    {
        uint8_t * device_address =
            (uint8_t *)(uintptr_t)
            (APP_NOR_MMAP_BASE + address);

       fsp_err_t err = g_spi_flash0.p_api->erase(
            g_spi_flash0.p_ctrl,
            device_address,
            APP_NOR_ERASE_SIZE);
        if (FSP_SUCCESS != err)
       {
            rc = -(int) err;
            break;
        }

       rc = rza3m_nor_wait_ready();

       if (rc != 0)
        {
           break;
        }

       address   += APP_NOR_ERASE_SIZE;
       remaining -= APP_NOR_ERASE_SIZE;
    }

    int end_rc = rza3m_nor_end();

    if ((rc == 0) && (end_rc != 0))
    {
        rc = end_rc;
   }

    return rc;
}

int rza3m_nor_write(uint32_t offset,
                  const void * source,
                  uint32_t length)
{
    if ((source == NULL) && (length != 0U))
    {
        return -1;
    }

    if ((offset > APP_NOR_SIZE) ||
        (length > (APP_NOR_SIZE - offset)))
    {
        return -1;
    }

    if (length == 0U)
    {
        return 0;
    }

    int rc = rza3m_nor_begin();

    if (rc != 0)
    {
        return rc;
    }

    const uint8_t * src = (const uint8_t *) source;
    uint32_t address = offset;
    uint32_t remaining = length;

    while (remaining > 0U)
    {
        uint32_t page_remaining =
            APP_NOR_PAGE_SIZE -
            (address & (APP_NOR_PAGE_SIZE - 1U));

        uint32_t chunk =
            (remaining < page_remaining) ?
            remaining :
            page_remaining;

        uint8_t * device_address =
            (uint8_t *)(uintptr_t)
            (APP_NOR_MMAP_BASE + address);

        fsp_err_t err = g_spi_flash0.p_api->write(
            g_spi_flash0.p_ctrl,
            src,
            device_address,
            chunk);

        if (FSP_SUCCESS != err)
        {
            rc = -(int) err;
            break;
        }

        rc = rza3m_nor_wait_ready();

        if (rc != 0)
        {
            break;
        }

        address   += chunk;
        src       += chunk;
        remaining -= chunk;
    }

    int end_rc = rza3m_nor_end();

    if ((rc == 0) && (end_rc != 0))
    {
        rc = end_rc;
    }

    return rc;
}

