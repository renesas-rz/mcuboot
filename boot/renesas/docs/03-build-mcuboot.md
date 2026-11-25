# 3. Build MCUboot in e² studio

MCUboot is built with its own Makefile, in the same way as the RZ/A IPL. e² studio runs `make` in `boot/renesas` with the arguments below.

> **Before you start:** set up MSYS2 and the `PATH` environment variable as described in [guide 2](02-prerequisites.md). Without it, `make` and `aarch64-none-elf-gcc` are not found.

## 3.1 Import the project

Import the repository following the same procedure as for the IPL in [IPL build environment construction procedure](https://github.com/renesas/rza-initial-program-loader/blob/main/application_note/06.%20IPL%20build%20environment%20construction%20procedure.md), with these differences:

| IPL guide | MCUboot |
|---|---|
| IPL repository folder | MCUboot repository root (`%MCUBOOT_DIR%`, the folder containing `boot/`, `scripts/`, `.project`) |
| Project name | **MCUboot** |
| Build directory | `${workspace_loc:/MCUboot/boot/renesas}` |
| Build configurations / make arguments | See 3.2 |

Leave **Copy projects into workspace** unchecked. The project must be named `MCUboot`.

## 3.2 Build configurations

Open **Project → Properties → C/C++ Build** and check:

| Setting | Value |
|---|---|
| Builder type | External builder (Gnu Make Builder) |
| Generate Makefiles automatically | **Off** |
| Build directory | `${workspace_loc:/MCUboot/boot/renesas}` |
| Toolchain | GCC for Renesas RZ (A-Profile AArch64 bare-metal) |

Build arguments (**Behavior** tab → *Build (Incremental build)*):

| Configuration | Make arguments | Output folder |
|---|---|---|
| Release | `LAYOUT_OK=1 MODE=1 LOG=3 V=1 -j8 all` | `build/release/` |
| Debug | `LAYOUT_OK=1 MODE=1 LOG=4 V=1 -j8 DEBUG=1 all` | `build/debug/` |

| Variable | Meaning |
|---|---|
| `LAYOUT_OK=1` | Confirms that `rza3m_flash_layout.h` was reviewed for your board. The build stops without it. |
| `MODE=1` | RAM-load with revert |
| `LOG=` | Log level. `3` = info, `4` = debug incl. flash driver messages. |
| `DEBUG=1` | `-O0 -g`, no inlining |
| `V=1` | Print full compiler commands |

## 3.3 Build

1. **Project → Build Configurations → Set Active → Release**.
2. **Project → Clean**, then **Project → Build Project**.

The console ends with the size summary of `build/release/rza3m_mcuboot.elf`.

## 3.4 Output files

| File | Use |
|---|---|
| `rza3m_mcuboot.srec` | Program into NOR at `0x20020000` |
| `rza3m_mcuboot.elf` | Symbols |
| `rza3m_mcuboot.bin` | Raw NOR image: IPL application header followed by the payload linked at DDR `0x40001000` |
| `rza3m_mcuboot.map` | Link map |
