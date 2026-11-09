/*
 * Copyright (c) 2024, Renesas Electronics Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Vendored from rza-initial-program-loader v2.3.1.
 * MCUboot port changes: page program, 64 KiB block erase, bounded status
 * polling and status-read error propagation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <drivers/delay_timer.h>
#include "errno.h"
#if (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1)
#include "pfc_regs.h"
#include "lib/mmio.h"
#endif /* (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1) */
#include "qspiflash_mx25l25645g_api.h"

/* Defaults */
static const bool DEFAULT_PBE_ENABLE = false;
static const uint32_t DEFAULT_TIMEOUT = 1000; /* usec */
static const uint32_t DEFAULT_SPI_FREQUENCY = 66666666U;
static const e_read_mode_t DEFAULT_READ_MODE = READ_MODE_QUAD;
static const e_dummy_cycles_t DEFAULT_DUMMY_CYCLES = DUMMY_CYCLES_6;
static const e_burst_read_wrap_depth_t DEFAULT_BURST_READ_WRAP_DEPTH = BURST_READ_WRAP_DISABLE;
static const e_output_driver_strength_t DEFAULT_OUTPUT_DRIVER_STRENGTH = OUTPUT_DRIVER_STRENGTH_30_OHMS;

static const uint32_t SPI_POST_RESET_WAIT = 50; /* usec */
static const uint32_t FLASH_RESET_RECOVERY_WAIT = 40; /* usec */

/* Program/erase geometry and timeouts (timeouts in usec, polled every 1 usec) */
#define FLASH_PAGE_SIZE           256U
#define FLASH_ERASE_SIZE          0x10000U
#define FLASH_3BYTE_ADDR_LIMIT    0x01000000U
#define PAGE_PROGRAM_TIMEOUT_US   10000U
#define BLOCK_ERASE_TIMEOUT_US    5000000U

#if (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1)
static const uint32_t MAX_FLASH_UNLOCK_RETRIES = 3;

static const uintptr_t WP_IO2_PIN_PORT_ADDR = PFC_P05;
static const uintptr_t WP_IO2_PIN_PORT_MODE_ADDR = PFC_PMC05;
static const uint32_t WP_IO2_PIN_BIT = BIT(3);
#endif

/* Static function pre-definition */
static int flash_open(xspidevice_ctrl_t * ctrl, xspidevice_cfg_t const * cfg);
static int flash_close(xspidevice_ctrl_t * ctrl);
static int flash_exec_op(xspidevice_ctrl_t * ctrl, xspi_op_t const * op, bool is_write);
static int flash_enter_xip(xspidevice_ctrl_t * ctrl);
static int flash_exit_xip(xspidevice_ctrl_t * ctrl);
static int flash_get_info(xspidevice_ctrl_t * ctrl, xspidevice_info_t * info);
static int flash_read(xspidevice_ctrl_t * ctrl, void * buffer, size_t address, size_t length);
static int flash_write(xspidevice_ctrl_t * ctrl, void const * buffer, size_t address, size_t length);
static int flash_erase(xspidevice_ctrl_t * ctrl, size_t address, size_t length);
static enum xspidevice_write_status flash_get_write_status(xspidevice_ctrl_t * ctrl);
static int flash_get_logical_address(xspidevice_ctrl_t * ctrl, size_t paddress, size_t * const laddress);

/* API function table definition */
const xspidevice_api_t qspiflash_mx25l25645g_api = {
	.open = flash_open,
	.close = flash_close,
	.exec_op = flash_exec_op,
	.enter_xip = flash_enter_xip,
	.exit_xip = flash_exit_xip,
	.get_info = flash_get_info,
	.read = flash_read,
	.write = flash_write,
	.erase = flash_erase,
	.get_write_status = flash_get_write_status,
	.get_logical_address = flash_get_logical_address,
};

/* Static variables */
static const int spi_frequency = DEFAULT_SPI_FREQUENCY + 1;

/*
 * Flash commands.
 * Commands used for program/erase/status use the same bus timing as the
 * validated FSP r_spibsc configuration: IO3 held high, SLCH=0, CLSH=0, SHSL=4.
 */

/* WREN (Write Enable) */
static const xspi_op_t op_write_enable = {
	.form = SPI_FORM_1_1_1,
	.op = 0x06,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 4
};

/* RDID (Read Manufacturer ID) */
static const xspi_op_t op_read_id = {
	.form = SPI_FORM_1_1_1,
	.op = 0x9f,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 4
};

/* RDSR (Read Status Register) */
static const xspi_op_t op_read_status = {
	.form = SPI_FORM_1_1_1,
	.op = 0x05,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 1,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 4
};

/* RDCR (Read Configuration Register) */
static const xspi_op_t op_read_configuraton = {
	.form = SPI_FORM_1_1_1,
	.op = 0x15,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 1,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* WRSR (Write Status/Configuration Register) */
static const xspi_op_t op_write_sr = {
	.form = SPI_FORM_1_1_1,
	.op = 0x01,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 2,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* RSTEN (Reset Enable) */
static const xspi_op_t op_reset_enable = {
	.form = SPI_FORM_1_1_1,
	.op = 0x66,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* RST (Reset) */
static const xspi_op_t op_reset = {
	.form = SPI_FORM_1_1_1,
	.op = 0x99,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* Burst Read (set wrap depth) */
static const xspi_op_t op_burst_read = {
	.form = SPI_FORM_1_1_1,
	.op = 0xc0,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 0,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 1,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* FASTREAD4B (Fast Read with 4-byte address, 1-1-1) */
static const xspi_op_t op_fast_read = {
	.form = SPI_FORM_1_1_1,
	.op = 0x0c,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 4,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 8, /* cannot change */
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* 4READ4B (4x I/O Read with 4-byte address, 1-4-4) */
static const xspi_op_t op_quad_io_read = {
	.form = SPI_FORM_1_4_4,
	.op = 0xec,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 4,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = (uint8_t) DEFAULT_DUMMY_CYCLES,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* PP4B (Page Program with 4-byte address) */
static const xspi_op_t op_page_program_4b = {
	.form = SPI_FORM_1_1_1,
	.op = 0x12,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 4,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 1
};

/* BE (64 KiB Block Erase with 3-byte address), as in the FSP r_spibsc configuration */
static const xspi_op_t op_block_erase_64k = {
	.form = SPI_FORM_1_1_1,
	.op = 0xD8,
	.op_size = 1,
	.op_is_ddr = false,
	.address = 0,
	.address_is_ddr = false,
	.address_size = 3,
	.additional_value = 0,
	.additional_size = 0,
	.dummy_cycles = 0,
	.transfer_buffer = NULL,
	.transfer_is_ddr = false,
	.transfer_size = 0,
	.force_idle_level_mask = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.force_idle_level_value = 0x08,	// Keep IO3/HOLDorRESET pin to High
	.slch_value = 0,
	.clsh_value = 0,
	.shsl_value = 4
};

/* Function definitions */
static int flash_read_status_register(qspiflash_mx25l25645g_ctrl_t * myctrl, uint8_t * status)
{
	const xspi_instance_t * xspi = myctrl->xspi;

	xspi_op_t op = op_read_status;
	op.transfer_buffer = status;
	int result = xspi->api->exec_op(xspi->ctrl, &op, false);
	if (result < 0) {
		ERROR("Failed to read status register\n");
	}
	return result;
}

static uint8_t flash_read_configuration_register(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	int result;
	const xspi_instance_t * xspi = myctrl->xspi;

	uint8_t configuration;
	xspi_op_t op = op_read_configuraton;
	op.transfer_buffer = &configuration;
	result = xspi->api->exec_op(xspi->ctrl, &op, false);
	if (result < 0) {
		ERROR("Failed to read configuration register\n");
		panic();
	}
	return configuration;
}

/* Poll WIP (status bit 0) until clear. timeout_us is the number of 1 usec polls. */
static int flash_wait_for_write_complete(qspiflash_mx25l25645g_ctrl_t * myctrl, uint32_t timeout_us)
{
	uint8_t status = 0U;

	for (uint32_t elapsed = 0U; elapsed < timeout_us; elapsed++) {
		int result = flash_read_status_register(myctrl, &status);
		if (result < 0) return result;
		if ((status & BIT(0)) == 0U) return 0;
		udelay(1U);
	}

	ERROR("Timeout waiting for write completion, status: 0x%02x\n", status);
	return -1;
}

static int flash_write_enable(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	const xspi_instance_t * xspi = myctrl->xspi;
	uint32_t timeout = DEFAULT_TIMEOUT;
	enum xspidevice_write_status status = WRITE_STATUS_DISABLED;

	while ((status != WRITE_STATUS_OK) && (timeout > 0U)) {
		xspi->api->exec_op(xspi->ctrl, &op_write_enable, false);
		status = flash_get_write_status(myctrl);
		timeout--;
		udelay(1);
	}
	if (status != WRITE_STATUS_OK) {
		ERROR("Write enable failed, status: %d\n", status);
		return -1;
	}
	return 0;
}

static int flash_check_manufacturer_id(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	int result;
	const xspi_instance_t * xspi = myctrl->xspi;

	uint8_t id[3];
	xspi_op_t op = op_read_id;
	op.transfer_buffer = id;
	op.transfer_size = 3;
	result = xspi->api->exec_op(xspi->ctrl, &op, false);
	if (result < 0) return result;
	const uint16_t device_id = (id[1] << 8) | id[2];
	if ((id[0] != MANUFACTURER_ID) || (device_id != DEVICE_ID)) {
		ERROR("Flash device not found\n");
		VERBOSE("Manufacturer ID: 0x%02x, Device ID: 0x%04x\n", id[0], device_id);
		return -1;
	}

	return 0;
}

static int flash_reset(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	const xspi_instance_t * xspi = myctrl->xspi;
	(void)flash_wait_for_write_complete(myctrl, DEFAULT_TIMEOUT);

	/* Reset Enable */
	int result = xspi->api->exec_op(xspi->ctrl, &op_reset_enable, false);
	if (result < 0) return result;

	/* Reset */
	result = xspi->api->exec_op(xspi->ctrl, &op_reset, false);
	if (result < 0) return result;

	udelay(FLASH_RESET_RECOVERY_WAIT);

	return result;
}

#if (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1)
static int flash_prepare_unlock(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	/* Set WP#SIO2 as GPIO */
	mmio_write_8(WP_IO2_PIN_PORT_MODE_ADDR, mmio_read_8(WP_IO2_PIN_PORT_MODE_ADDR) & ~WP_IO2_PIN_BIT);
	/* Set WP#SIO2 to high */
	mmio_write_8(WP_IO2_PIN_PORT_ADDR, mmio_read_8(WP_IO2_PIN_PORT_ADDR) | WP_IO2_PIN_BIT);

	/* Check if WP#SIO2 is high */
	if ((mmio_read_8(WP_IO2_PIN_PORT_ADDR) & WP_IO2_PIN_BIT) == 0) {
		ERROR("Failed to pull up WP#SIO2\n");
		return -1;
	}
	return 0;
}

static int flash_finish_unlock(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	/* Set WP#SIO2 as peripheral */
	mmio_write_8(WP_IO2_PIN_PORT_MODE_ADDR, mmio_read_8(WP_IO2_PIN_PORT_MODE_ADDR) | WP_IO2_PIN_BIT);
	return 0;
}

static void flash_hw_protect_unlock(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	VERBOSE("Trying to unlock the flash HW protection\n");

	int result = flash_reset(myctrl);
	if (result < 0) {
		ERROR("Failed to reset the flash while unlocking HW protection\n");
		goto err;
	}

	/* Write enable */
	result = flash_write_enable(myctrl);
	if (result < 0) {
		ERROR("Failed to enable write while unlocking HW protection\n");
		goto err;
	}

	const xspi_instance_t * xspi = myctrl->xspi;

	if (flash_prepare_unlock(myctrl) < 0) goto err;

	xspi_op_t op = op_write_sr;
	uint8_t status_configuration_register[2] = {
		0, /* Status Register, ensure the SRWD(bit7) is 0 */
		0,
	};
	op.transfer_buffer = (void *)&status_configuration_register;
	result = xspi->api->exec_op(xspi->ctrl, &op, true);
	if (result < 0) goto err;

	result = flash_wait_for_write_complete(myctrl, DEFAULT_TIMEOUT);
	if (result < 0) goto err;

	uint8_t status = 0U;
	result = flash_read_status_register(myctrl, &status);
	if (result < 0) goto err;

	if (flash_finish_unlock(myctrl) < 0) goto err;

	VERBOSE("Flash HW protection is unlocked\n");

	return;

err:
	ERROR("Failed to unlock the flash\n");
	panic();
}
#endif /* (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1) */

static int flash_init(qspiflash_mx25l25645g_ctrl_t * myctrl)
{
	if (flash_check_manufacturer_id(myctrl) != 0) return -1;

	const xspi_instance_t * xspi = myctrl->xspi;

	int result = flash_reset(myctrl);
	if (result < 0) return result;

	/* Write enable */
	result = flash_write_enable(myctrl);
	if (result < 0) return result;

	/* Determine status_register */
	uint8_t status_register = 0;
	if (DEFAULT_READ_MODE == READ_MODE_QUAD) {
		status_register = 1u << 6; /* QE (Quad Enable) : Enable */
	}

	/* Determine configuration_register */
	uint8_t configuration_register = 0;
	if (DEFAULT_READ_MODE == READ_MODE_QUAD) {
		switch (DEFAULT_DUMMY_CYCLES) {
		case DUMMY_CYCLES_4:
			configuration_register = 1u << 6;
			break;
		case DUMMY_CYCLES_8:
			configuration_register = 2u << 6;
			break;
		case DUMMY_CYCLES_10:
			configuration_register = 3u << 6;
			break;
		case DUMMY_CYCLES_6:
		default:
			configuration_register = 0u << 6; /* Default: 6 dummy cycles */
			break;
		}
	} else {
		if (DEFAULT_DUMMY_CYCLES != DUMMY_CYCLES_8) {
			ERROR("Only dummy cycles 8 is supported for normal read mode\n");
			return -1;
		}

		configuration_register = 0u << 6; /* DC[1:0] */
	}

	if (DEFAULT_PBE_ENABLE) {
		configuration_register |= 1u << 4; /* PBE (Preamble Bit Enable) */
	}
	configuration_register |= DEFAULT_OUTPUT_DRIVER_STRENGTH << 0; /* ODS[1:0] */

	/* Write configuration register */
	xspi_op_t op = op_write_sr;
	uint8_t status_configuration_register[2] = {
		status_register,
		configuration_register
	};
	op.transfer_buffer = (void *)&status_configuration_register;
	result = xspi->api->exec_op(xspi->ctrl, &op, true);
	if (result < 0) return result;

	result = flash_wait_for_write_complete(myctrl, DEFAULT_TIMEOUT);
	if (result < 0) return result;

	/* Verify if the status register is written correctly */
	uint8_t status = 0U;
	result = flash_read_status_register(myctrl, &status);
	if (result < 0) return result;

	if (status != status_register) {
#if (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1)
		if ((status & 0x80) != 0) {
			WARN("Flash is in Hardware Protection Mode\n");
			return -EAGAIN;
		}
#endif
		ERROR("Failed to write status register\n");
		return -1;
	}

	/* Verify if the configuration register is written correctly */
	uint8_t configuration = flash_read_configuration_register(myctrl);
	if (configuration != configuration_register) {
		ERROR("Failed to write configuration register\n");
		return -1;
	}

	return result;
}

static int flash_open(xspidevice_ctrl_t * ctrl, xspidevice_cfg_t const * cfg)
{
	assert(ctrl);
	assert(cfg);
	int result;
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = cfg->xspi;

	if (myctrl->opened) return -EBUSY;

	myctrl->xspi = xspi;
	result = xspi->api->open(xspi->ctrl, xspi->cfg);
	if (result == 0) {
		result = xspi->api->set_frequency(xspi->ctrl, spi_frequency);
	}
	if (result == 0) {
		udelay(SPI_POST_RESET_WAIT);

#if (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1)
		uint32_t retries = 0;
		do {
			retries++;
			if (retries >= MAX_FLASH_UNLOCK_RETRIES) break;

			result = flash_init(myctrl);
			if (result == -EAGAIN) flash_hw_protect_unlock(myctrl);
		} while (result == -EAGAIN);
#else /* (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1) */
		result = flash_init(myctrl);
#endif /* (RZA_UNLOCK_FLASH_WRITE_PROTECT == 1) */

		if (result == 0) {
			myctrl->opened = true;
		}
		else {
			xspi->api->close(xspi->ctrl);
		}
	}

	return result;
}

static int flash_close(xspidevice_ctrl_t * ctrl)
{
	assert(ctrl);
	int result = -1;
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;
	if (myctrl->opened) {
		result = xspi->api->close(xspi->ctrl);
		myctrl->opened = false;
	}

	return result;
}

static int flash_exec_op(xspidevice_ctrl_t * ctrl, xspi_op_t const * op, bool is_write)
{
	assert(ctrl);
	assert(op);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;

	return xspi->api->exec_op(xspi->ctrl, op, is_write);
}

static int flash_enter_xip(xspidevice_ctrl_t * ctrl)
{
	assert(ctrl);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;

	int result = flash_write_enable(myctrl);
	if (result < 0) return result;

	xspi_op_t op = op_burst_read;
	uint8_t wrap_depth = DEFAULT_BURST_READ_WRAP_DEPTH;
	op.transfer_buffer = (void *)&wrap_depth;
	result = xspi->api->exec_op(xspi->ctrl, &op, true);
	if (result < 0) return result;

	result = flash_wait_for_write_complete(myctrl, DEFAULT_TIMEOUT);
	if (result < 0) return result;

	if (DEFAULT_READ_MODE == READ_MODE_QUAD) {
		result = xspi->api->configure_xip(xspi->ctrl, &op_quad_io_read, NULL);
	} else {
		result = xspi->api->configure_xip(xspi->ctrl, &op_fast_read, NULL);
	}
	if (result < 0) return result;

	result = xspi->api->set_frequency(xspi->ctrl, spi_frequency);
	if (result < 0) return result;

	return xspi->api->start_xip(xspi->ctrl);
}

static int flash_exit_xip(xspidevice_ctrl_t * ctrl)
{
	assert(ctrl);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;

	xspi_op_t op = op_burst_read;
	uint8_t wrap_depth = BURST_READ_WRAP_DISABLE;
	op.transfer_buffer = (void *)&wrap_depth;
	int result = xspi->api->exec_op(xspi->ctrl, &op, true);
	if (result < 0) return result;

	result = flash_wait_for_write_complete(myctrl, DEFAULT_TIMEOUT);
	if (result < 0) return result;

	return xspi->api->stop_xip(xspi->ctrl);
}

static int flash_get_info(xspidevice_ctrl_t * ctrl, xspidevice_info_t * info)
{
	assert(ctrl);
	assert(info);

	info->capacity = 32U * 1024U * 1024U;
	info->minimum_erase_size = FLASH_ERASE_SIZE;
	strlcpy(info->device_vendor, "Macronix", sizeof(info->device_vendor));
	strlcpy(info->device_product, "MX25L25645G", sizeof(info->device_product));

	return 0;
}

static int flash_read(xspidevice_ctrl_t * ctrl, void * buffer, size_t address, size_t length)
{
	assert(ctrl);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;

	uintptr_t mmap_base = xspi->api->get_mmap_base(xspi->ctrl);
	memcpy(buffer, (void *)(mmap_base + address), length);

	return 0;
}

static int flash_write(xspidevice_ctrl_t * ctrl, void const * buffer, size_t address, size_t length)
{
	assert(ctrl);
	assert((buffer != NULL) || (length == 0U));
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;
	const uint8_t * source = (const uint8_t *)buffer;

	if (!myctrl->opened) return -1;

	while (length > 0U) {
		/* Page Program must not cross a page boundary */
		size_t chunk = FLASH_PAGE_SIZE - (address & (FLASH_PAGE_SIZE - 1U));
		if (chunk > length) chunk = length;

		int result = flash_write_enable(myctrl);
		if (result < 0) return result;

		xspi_op_t op = op_page_program_4b;
		op.address = address;
		op.transfer_buffer = (void *)source;
		op.transfer_size = chunk;
		result = xspi->api->exec_op(xspi->ctrl, &op, true);
		if (result < 0) return result;

		result = flash_wait_for_write_complete(myctrl, PAGE_PROGRAM_TIMEOUT_US);
		if (result < 0) return result;

		address += chunk;
		source += chunk;
		length -= chunk;
	}

	return 0;
}

static int flash_erase(xspidevice_ctrl_t * ctrl, size_t address, size_t length)
{
	assert(ctrl);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	const xspi_instance_t * xspi = myctrl->xspi;

	if (!myctrl->opened) return -1;

	if (((address & (FLASH_ERASE_SIZE - 1U)) != 0U) ||
	    ((length & (FLASH_ERASE_SIZE - 1U)) != 0U)) {
		return -1;
	}

	/* The erase command uses 3-byte addressing */
	if ((address >= FLASH_3BYTE_ADDR_LIMIT) || (length > (FLASH_3BYTE_ADDR_LIMIT - address))) {
		return -1;
	}

	while (length > 0U) {
		int result = flash_write_enable(myctrl);
		if (result < 0) return result;

		xspi_op_t op = op_block_erase_64k;
		op.address = address;
		result = xspi->api->exec_op(xspi->ctrl, &op, false); /* no data phase */
		if (result < 0) return result;

		result = flash_wait_for_write_complete(myctrl, BLOCK_ERASE_TIMEOUT_US);
		if (result < 0) return result;

		address += FLASH_ERASE_SIZE;
		length -= FLASH_ERASE_SIZE;
	}

	return 0;
}

static enum xspidevice_write_status flash_get_write_status(xspidevice_ctrl_t * ctrl)
{
	assert(ctrl);
	qspiflash_mx25l25645g_ctrl_t * myctrl = (qspiflash_mx25l25645g_ctrl_t *)ctrl;
	uint8_t status = 0U;

	if (flash_read_status_register(myctrl, &status) < 0) return WRITE_STATUS_DISABLED;
	if ((status & BIT(0)) != 0U) return WRITE_STATUS_IN_PROGRESS;
	if ((status & BIT(1)) == 0U) return WRITE_STATUS_DISABLED;

	return WRITE_STATUS_OK;
}

static int flash_get_logical_address(xspidevice_ctrl_t * ctrl, size_t paddress, size_t * const laddress)
{
	(void)ctrl;
	(void)paddress;
	(void)laddress;
	return -1;
}
