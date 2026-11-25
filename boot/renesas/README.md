# MCUboot for Renesas RZ/A3M

MCUboot port for the RZ/A3M (R9A07G066M04GBG) on the **EK-RZ/A3M** board, booting from QSPI NOR.

> MCUboot supports RAM-load boot, image validation, A/B selection and trial rollback. Use e² studio or J-Flash Lite to program the bootloader; e² studio also supports debugging. See [Known limitations](docs/08-limitations.md).

```text
Boot ROM -> IPL (NOR 0x20000000) -> MCUboot (NOR 0x20020000, runs in DDR 0x40001000)
        -> validates slot A / slot B -> copies the image to DDR 0x401FF000 -> jumps to 0x40200000
```

## Documentation

| # | Guide | Content |
|---|---|---|
| 1 | [Overview](docs/01-overview.md) | Boot flow, memory map, boot modes |
| 2 | [Prerequisites](docs/02-prerequisites.md) | Tools, versions, build environment, `MCUBOOT_DIR` |
| 3 | [Build MCUboot in e² studio](docs/03-build-mcuboot.md) | Import, build configurations, output files |
| 4 | [Program and debug the board](docs/04-flash-and-debug.md) | e² studio and J-Flash Lite |
| 5 | [Create the application](docs/05-application.md) | FSP Blinky, SCIF, SPIBSC, MCUboot files, example code |
| 6 | [Sign and program the application](docs/06-sign-and-flash-app.md) | `sign_image.cmd`, slots |
| 7 | [Troubleshooting](docs/07-troubleshooting.md) | Common build and boot errors |
| 8 | [Known limitations](docs/08-limitations.md) | What is not supported yet |

## Quick start

1. Set up the build environment and `MCUBOOT_DIR` ([guide 2](docs/02-prerequisites.md)).
2. Set up the e² studio project **MCUboot** and build **Release** ([guide 3](docs/03-build-mcuboot.md)).
3. Program the IPL with J-Flash Lite and MCUboot using e² studio or J-Flash Lite ([guide 4](docs/04-flash-and-debug.md)).
4. Build an FSP application, package it and program it to slot A with J-Flash Lite ([guide 5](docs/05-application.md) and [guide 6](docs/06-sign-and-flash-app.md)).
