# 4. Program and debug the board

Use **e² studio** with J-Link to program and debug MCUboot, or **J-Flash Lite** from SEGGER J-Link Software **V9.44** to program NOR.

## 4.1 Program and debug with e² studio

1. Build the MCUboot configuration you want to debug ([guide 3](03-build-mcuboot.md)). Set the EK-RZ/A3M to NOR boot mode and connect the J-Link. If the IPL is not already programmed, program it first using section 4.2.
2. Open **Run → Debug Configurations → Renesas GDB Hardware Debugging** and create a configuration for project `MCUboot`.
3. On **Main**, select the corresponding MCUboot ELF: `boot/renesas/build/debug/rza3m_mcuboot.elf` for Debug or `boot/renesas/build/release/rza3m_mcuboot.elf` for Release.
4. On **Debugger → Debug hardware**, select J-Link ARM, target `R9A07G066M04GBG`, board **EK-RZ/A3M NOR Boot (Exec with DDR SDRAM)** and interface **JTAG**. Enable **Reset at the end of connection** and **Reset after download**.
5. On **Startup**, configure *Load image and symbols* to download the matching MCUboot S-record (`boot/renesas/build/debug/rza3m_mcuboot.srec` or `boot/renesas/build/release/rza3m_mcuboot.srec`) and load symbols from the matching ELF. The S-record contains the NOR address; do not download the ELF as a separate NOR image.
6. Launch the configuration and check that the download completes successfully. Use breakpoints, stepping and the memory/register views to debug. The IPL and an application image must also be programmed for the normal boot flow.

To check the normal boot chain independently of the debugger, power-cycle the board after programming. A debugger reset and a cold power-cycle need not exercise the same boot path.

## 4.2 Program with J-Flash Lite

1. Set the EK-RZ/A3M to NOR boot mode and connect the J-Link.
2. Start **J-Flash Lite** (V9.44).
3. Select device `R9A07G066M04GBG`, interface **JTAG**, speed **Auto**, then **OK**.
4. Program the files in this order. S-records contain their address. For `.bin` files, enter the address in **Prog. addr.**

| # | File | Address |
|---|---|---|
| 1 | `rza3m_ek_nor_ipl.srec` (IPL) | in file (`0x20000000`) |
| 2 | `boot/renesas/build/release/rza3m_mcuboot.srec` | in file (`0x20020000`) |
| 3 | Packaged application for slot A, e.g. `Blinky_A.bin` | `0x20100000` |
| 4 | Packaged application for slot B (optional) | `0x20200000` |

Signing the application is described in [guide 6](06-sign-and-flash-app.md). Before programming a slot, erase the whole slot (1 MiB), including its last 64 KiB sector that holds the trailer.

5. **Power-cycle** the board.
6. The console shows:

```text
[INF] RZ/A3M MCUboot - mode ram-load+revert
[INF] Primary   slot: ...
```

With no application programmed, MCUboot reports `no bootable image` and halts.
