# 7. Troubleshooting

## Build

| Symptom | Cause / fix |
|---|---|
| `make: *** ... LAYOUT_OK` | Add `LAYOUT_OK=1` after reviewing `rza3m_flash_layout.h`. |
| `make` or `aarch64-none-elf-gcc` not found | MSYS2 or the toolchain is not on `PATH`. Follow the IPL environment setup referenced in guide 2, then restart e² studio. |
| Application: `conflicting types for '__errno'` in `bsp_sbrk.c` | FSP 4.2.0 `bsp_sbrk.c` declares a local `errno`. Copy it to `src/`, rename the variable, and exclude the original from the build. |
| Application: standard headers replaced, `errno`/`assert` errors | An MCUboot folder other than those listed in guide 5.5 is on the include path. Remove it. |
| Application: linked files or includes not found | `MCUBOOT_DIR` is not defined, or e² studio was not restarted after defining it. |
| Application: `undefined reference to rza3m_plat_putc` | Add the console code from guide 5.6. |
| Application: `undefined reference to rza3m_nor_*` | Link `boot/renesas/app/rza3m_app_nor_fsp.c`. |

## Signing

| Symptom | Fix |
|---|---|
| `Microsoft Visual C++ 14.0 or greater is required` | Python too new (e.g. 3.15), no prebuilt wheels. Use Python 3.12 or 3.13 and `set PYTHON=py -3.12`. |
| `imgtool not found` | Set `MCUBOOT_DIR`. |
| `objcopy failed` | Set `OBJCOPY` to the toolchain's `aarch64-none-elf-objcopy.exe`. |
| `entry 0x... expected 0x40200000` | The application is not linked at `0x40200000`, see guide 5.2. |
| `binary is ... bytes` | A section has a load address outside DDR. Check the linker script. |

## Boot

| Symptom | Cause / fix |
|---|---|
| `no bootable image` | No valid signed image in either slot. Check that the signed `.bin` was programmed at the slot address. |
| `SCIF Download mode due to error` after a reset during NOR erase | This can occur before IPL starts. Let NOR settle, then reset again. In the [live erase reset test](TEST_PLAN.md#ram-loadrevert-live-erase-reset), a second reset after a three-second wait recovered without reflashing. |
| New image in slot B not booted | Its version is not higher than slot A. |
| Image boots once, then is erased | Trial image not confirmed. Check `rza3m_app_confirm()` and that `HEALTH_FORCE_FAIL` is not defined. |
| Programming from e² studio fails or the session disconnects | Not supported. Program with J-Flash Lite and power-cycle. |
