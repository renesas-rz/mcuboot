# 2. Prerequisites

## Hardware

- EK-RZ/A3M evaluation board, NOR boot mode
- SEGGER J-Link (JTAG)
- USB-serial connection to the SCIF0 console, 115200 8N1

## Software

| Tool | Validated version |
|---|---|
| e² studio for RZ | 2026-07 |
| RZ/A FSP | 4.2.0 |
| Toolchain | Arm GNU Toolchain 13.2.Rel1 `aarch64-none-elf` (bundled with e² studio) |
| MSYS2 | Provides `make` and the shell tools used by the Makefile |
| SEGGER J-Link Software, incl. **J-Flash Lite** | **V9.44** |
| Python | **3.12 or 3.13**, 64-bit. Python 3.15 is not supported, see [Troubleshooting](07-troubleshooting.md). |
| Git | Any recent version |

## Build environment (MSYS2 and PATH)

MCUboot is built with the same environment as the RZ/A IPL. Install MSYS2 and add MSYS2 and the toolchain to the Windows `PATH` exactly as described in the IPL application note:

- [IPL: Install Additional Software for Windows 10](https://github.com/renesas/rza-initial-program-loader/blob/main/application_note/05.02.01%20Install%20Additional%20Software%20for%20Windows%2010.md)
- [IPL: IPL build environment construction procedure](https://github.com/renesas/rza-initial-program-loader/blob/main/application_note/06.%20IPL%20build%20environment%20construction%20procedure.md)

Then open a new Command Prompt and check that both tools are found:

```cmd
make --version
aarch64-none-elf-gcc --version
```

Restart e² studio after changing `PATH`.

## `MCUBOOT_DIR` environment variable

Define a global Windows environment variable pointing to the MCUboot repository. The application project uses it to link the MCUboot files, and `sign_image.cmd` uses it to find `imgtool`.

| Name | Example value |
|---|---|
| `MCUBOOT_DIR` | `C:\work\mcuboot` |

1. **Windows Settings → System → About → Advanced system settings → Environment Variables**.
2. Under **User variables** (or **System variables**), click **New**.
3. Name `MCUBOOT_DIR`, value = path of the cloned repository, no trailing backslash.
4. Restart e² studio and any open Command Prompt.

Check:

```cmd
echo %MCUBOOT_DIR%
dir %MCUBOOT_DIR%\boot\renesas
```

## Python packages for imgtool

```cmd
cd %MCUBOOT_DIR%
py -3.12 -m pip install -r scripts\requirements.txt
py -3.12 scripts\imgtool.py version
```

## IPL S-record

You also need the RZ/A3M IPL S-record (`rza3m_ek_nor_ipl.srec`) for the EK-RZ/A3M NOR target. It is programmed together with MCUboot, see [guide 4](04-flash-and-debug.md).
