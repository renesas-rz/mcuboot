# 5. Create the application

The application is a normal RZ/A FSP project, linked to run from DDR `0x40200000`, plus a few MCUboot files to read and confirm its boot state.

## 5.1 Create the Blinky project

Follow the RZ/A FSP *Getting Started* guide to create a Blinky project:

1. **File → New → Renesas C/C++ Project → Renesas RZ → RZ/A C/C++ FSP Project**.
2. Project name `Blinky_RZA3M`, board **EK-RZA3M NOR Boot**, device `R9A07G066M04GBG`.
3. Toolchain **GCC ARM A-Profile AArch64 13.2.1**.
4. Build artifact **Executable**, RTOS **No RTOS**, template **Bare Metal - Blinky**.
5. Build once to check the project compiles unchanged.

## 5.2 Run address (optional)

MCUboot copies the image so that the code starts at `0x40200000`. **The example Blinky project is already linked there, so this step is only needed for a new project.**

1. Open `script/rza3m_ek_nor.ld` and set the DDR execution start of the program to `0x40200000`.
2. Rebuild. In `Debug/Blinky_RZA3M.map`, check that the first code section starts at `0x40200000`.

When an FSP loader header is present, `sign_image.cmd` checks its entry and copy addresses and stops if they are not `0x40200000`. Without that header, it warns that the entry address was not checked.

## 5.3 Add the SCIF console

1. Open `configuration.xml` → **Stacks → New Stack → Connectivity → UART (r_scif_uart)**.
2. Name `g_uart0`, **channel 0**, 115200 8N1, callback `uart_callback`.
3. **Pins** tab: keep the **default SCIF0 TX/RX pins** of the EK-RZA3M NOR Boot board.
4. **Generate Project Content**.

## 5.4 Add the SPIBSC flash driver

Needed for `rza3m_app_confirm()` and image updates from the application.

1. **Stacks → New Stack → Storage → QSPI (r_spibsc)**, name `g_spi_flash0`.
2. Keep the defaults below. They were validated on EK-RZ/A3M with the MX25L25645G:

| Property | Value |
|---|---|
| SPI Protocol | Extended SPI |
| Address Bytes | 3 |
| Read Mode | Fast Read Quad I/O |
| Page Size Bytes | 256 |
| Page Program Command | `0x38`, address lines 4 |
| Write Enable / Read Status | `0x06` / `0x05`, status bit 0 |
| Erase commands | `0x20` 4 KiB, `0x52` 32 KiB, `0xD8` 64 KiB, `0xC7` chip |
| IO3 fix | Enable, High |

3. **BSP** tab → set **Heap size** to a non-zero value. `R_SPIBSC_Open()` uses `malloc()`.
4. **Generate Project Content**.

### Watchdog stack (if used by the application)

To start a watchdog from the application, add a **Watchdog Timer (r_wdt)**
stack in `configuration.xml` → **Stacks → New Stack**. Name the instance
`g_wdt0`, configure its timeout and interrupt/callback settings for the
application, then select **Generate Project Content**. The generated
`g_wdt0_ctrl` and `g_wdt0_cfg` are used with `R_WDT_Open()` and
`R_WDT_Refresh()`. If using it to reset the board, configure the WDT0 system
reset route as well; the Blinky reference sets `CPG_WDTRST_SEL.WDTRSTSEL0`.

MCUboot starts its trial watchdog independently of this FSP stack. When the
application inherits a running WDT0, its timeout is already configured and
cannot be changed until reset. Do not open it again with FSP; preserve its
clocks and follow the trial handoff in section 5.6. The Blinky reference
refreshes the inherited watchdog through the WDT0 register after confirmation.

## 5.5 Add the MCUboot files

The application links the MCUboot files from the repository instead of copying them, so it always uses the same layout and trailer format as MCUboot. All paths use the `MCUBOOT_DIR` environment variable defined in [guide 2](02-prerequisites.md).

### Linked sources

Right-click `src` → **New → File → Advanced → Link to file in the file system**, and enter each path:

| File | Purpose |
|---|---|
| `${env_var:MCUBOOT_DIR}/boot/renesas/app/rza3m_app_boot.c` | Application API |
| `${env_var:MCUBOOT_DIR}/boot/renesas/app/rza3m_app_nor_fsp.c` | NOR backend on the FSP `r_spibsc` driver |
| `${env_var:MCUBOOT_DIR}/boot/renesas/src/flash_map_backend.c` | Flash areas |
| `${env_var:MCUBOOT_DIR}/boot/renesas/src/rza3m_console.c` | `rza3m_log()` formatter |
| `${env_var:MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c` | Trailer read/write |

If e² studio does not resolve `${env_var:...}` in the link dialog, click **Variables... → Edit Variables → New**, create a linked-resource variable `MCUBOOT_DIR` with the same value, and link with `MCUBOOT_DIR/boot/...`.

### NOR backend: `rza3m_app_nor_fsp.c`

`MCUBOOT_DIR/boot/renesas/app/rza3m_app_nor_fsp.c` is the provided FSP NOR backend for the application. It implements the functions the MCUboot sources call, on top of `g_spi_flash0`:

> **Caution:** Cache coherency after application writes and erases requires further validation; see [Known limitations](08-limitations.md).

```c
int rza3m_nor_read (uint32_t off, void *dst, uint32_t len);
int rza3m_nor_write(uint32_t off, const void *src, uint32_t len);
int rza3m_nor_erase(uint32_t off, uint32_t len);
```

It expects the FSP instance to be named `g_spi_flash0` (5.4).

### Include paths

**Project → Properties → C/C++ Build → Settings → Cross ARM C Compiler → Includes**, add:

```text
"${env_var:MCUBOOT_DIR}/boot/bootutil/include"
"${env_var:MCUBOOT_DIR}/boot/renesas/include"
"${env_var:MCUBOOT_DIR}/boot/renesas/include/mcuboot_config"
"${env_var:MCUBOOT_DIR}/boot/renesas/include/flash_map_backend"
"${env_var:MCUBOOT_DIR}/boot/renesas/app"
```

Do not add any other folder from `boot/renesas`. Only the paths above are meant for the application.

### Preprocessor symbols

**Cross ARM C Compiler → Preprocessor**:

| Symbol | Purpose |
|---|---|
| `MCUBOOT_APP_BUILD` | Builds the MCUboot sources in application mode |
| `HEALTH_FORCE_FAIL` | **Test only.** Makes the example health check fail, to test revert. Do not define in normal builds. |
| `APP_TEST_HANG` | **Test only.** Hangs an unconfirmed trial before confirmation to test watchdog recovery. |
| `APP_TEST_WATCHDOG_REFRESH` | **Test only.** Logs once after 20 seconds of refreshing the inherited watchdog. |
| `APP_REQUEST_TRIAL_ON_BOOT` | **Demo only.** Enables the startup request to trial the inactive slot and reboot. Leave it off for normal boots and watchdog tests. |

Apply the same settings to every build configuration you use.

## 5.6 Application code

The following extracts are from the example `src/hal_entry.c`.

In RAM-load+revert mode, MCUboot starts the watchdog before handing off to an
unconfirmed trial. Blinky does not reopen an inherited watchdog. It refreshes
the watchdog from the main loop only after the health check and confirmation
succeed. If either fails, Blinky stops without refreshing so MCUboot can roll
back on the next boot. MCUboot does not arm the trial watchdog for a confirmed
image.

On RZ/A3M, FSP's `bsp_clock_off()` runs before `hal_entry()` and normally
gates the WDT0 clocks. In Blinky's
`rz/fsp/src/bsp/mcu/rza3m/bsp_clocks_setup.c`, skip that gate when the clock
monitor is active and `WDTCNT.WDTEN` is set. Keep this adjustment when updating
FSP. For a watchdog started by the application itself, continue to use
`R_WDT_Open()` and `R_WDT_Refresh()`; do not reopen an inherited watchdog.

### Console output

`rza3m_console.c` formats the text and calls `rza3m_plat_putc()` for each character. Implement it with the SCIF driver:

```c
static volatile bool g_tx_done;

void uart_callback(uart_callback_args_t *p_args)
{
    if (p_args->event == UART_EVENT_TX_COMPLETE)
    {
        g_tx_done = true;
    }
}

void rza3m_plat_putc(char c)
{
    static uint8_t ch;

    ch = (uint8_t) c;
    g_tx_done = false;
    if (FSP_SUCCESS == R_SCIF_UART_Write(&g_uart0_ctrl, &ch, 1U))
    {
        while (!g_tx_done) { }
    }
}
```

The callback name must match the **Callback** property of `g_uart0`.

### Start-up

Open the UART before the first log message:

```c
void hal_entry(void)
{
    fsp_err_t err = R_SCIF_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        while (1) { }
    }

    rza3m_log("\nBlinky started from MCUboot\n");
    run_mcuboot_test();

    /* ... existing Blinky timer / LED code ... */
}
```

Keep the generated `R_BSP_WarmStart()`. It opens the MMU and the I/O ports.

### Health check and confirmation

A trial image (`test`) must confirm itself, otherwise MCUboot reverts to the previous image on the next reset. Confirm only after a health check:

```c
static void run_mcuboot_test(void)
{
    bool confirmed = false;
    int  rc = rza3m_app_is_confirmed(&confirmed);

    if ((rc != 0) || confirmed)
    {
        return;                         /* error, or already confirmed */
    }

    if (app_health_check() != 0)
    {
        rza3m_log("[APP] health FAILED - image will not be confirmed\n");
        return;                         /* MCUboot reverts on the next reset */
    }

    rc = rza3m_app_confirm();
    rza3m_log("[APP] confirm rc=%d\n", rc);
}
```

`app_health_check()` is application specific. The example checks a DDR buffer and that the generic timer is running, and returns `-99` when `HEALTH_FORCE_FAIL` is defined.

Do not call `rza3m_app_is_confirmed()` right after `rza3m_app_confirm()`. The memory-mapped NOR window may still show the old value. Check the state after the next reset.

### API reference

| Function | Purpose |
|---|---|
| `rza3m_app_boot_info()` | Running slot and version |
| `rza3m_app_is_confirmed()` | Whether the running image is confirmed |
| `rza3m_app_confirm()` | Confirm the running image |
| `rza3m_app_erase_inactive()` / `rza3m_app_write_inactive()` | Write an update into the other slot |
| `rza3m_app_mark_pending()` | Request a trial boot of the other slot, then reset |

Build the project. Signing is described in the next guide.
