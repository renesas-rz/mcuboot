# 6. Sign and program the application

MCUboot only boots images with an MCUboot header and trailer. `boot/renesas/scripts/sign_image.cmd` (Windows) and `sign_image.sh` (Linux/Git Bash) do the conversion:

```text
Blinky_RZA3M.elf -> raw binary -> strip FSP loader header -> imgtool sign -> signed .bin
```

## 6.1 Usage

```text
sign_image.cmd <ramload|swap> <factory|test|raw> <in.elf|in.bin> <version> <ui_bank> <out.bin>
```

| Argument | Values |
|---|---|
| mode | `ramload` (MCUboot `MODE=1`) |
| kind | `factory` – confirmed image, use for the first image in slot A<br>`test` – trial image, must be confirmed by the application<br>`raw` – no trailer, for OTA payloads written by the application |
| version | `1.0.0`, `1.0.1+5`, … must be higher than the running image |
| ui_bank | `0` for slot A, `1` for slot B |
| out.bin | Signed image to program into the slot |

Environment variables:

| Variable | Default | Set when |
|---|---|---|
| `MCUBOOT_DIR` | the global variable from guide 2, else three folders above the script | — |
| `PYTHON` | `py -3`, `python`, `python3` | several Pythons are installed, e.g. `py -3.12` |
| `OBJCOPY` | `aarch64-none-elf-objcopy` | the input is an `.elf` and the toolchain is not on `PATH` |
| `SIGNING_KEY` | — | MCUboot is built with signature verification |

## 6.2 Sign the image

Run from a Command Prompt after building the application:

```cmd
set PYTHON=py -3.12
cd <workspace>\Blinky_RZA3M

rem first image, slot A
%MCUBOOT_DIR%\boot\renesas\scripts\sign_image.cmd ramload factory Debug\Blinky_RZA3M.elf 1.0.0 0 Debug\Blinky_A.bin

rem update test image, slot B
%MCUBOOT_DIR%\boot\renesas\scripts\sign_image.cmd ramload test Debug\Blinky_RZA3M.elf 1.0.1 1 Debug\Blinky_B.bin
```

Expected end of the output:

```text
[sign] FSP header: entry=0x40200000 copy=0x40200000 size=0x...
[sign] SUCCESS
```

## 6.3 Program the image

Program the signed `.bin` with J-Flash Lite (V9.44) at the slot address, see [guide 4](04-flash-and-debug.md):

| Image | File | Address |
|---|---|---|
| First image | `Blinky_A.bin` (`factory`) | `0x20100000` (slot A) |
| Update test | `Blinky_B.bin` (`test`) | `0x20200000` (slot B) |

Before programming a new test image, erase the whole target slot including its last 64 KiB sector (the trailer). Otherwise an old trailer may cause MCUboot to reject the image.

Power-cycle and check the console:

```text
[INF] boot slot A  v1.0.0+0  ui_bank 0  entry 0x40200000
Blinky started from MCUboot
```
