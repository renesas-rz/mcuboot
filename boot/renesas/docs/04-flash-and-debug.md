# 4. Program the board

Use **J-Flash Lite** from SEGGER J-Link Software **V9.44** to program NOR.

> **Programming from e² studio does not work for this port, and debugging is experimental.**
> An e² studio debug session can only read registers and memory, and step through the code once. Do not use it to program the board.

## 4.1 Program with J-Flash Lite

1. Set the EK-RZ/A3M to NOR boot mode and connect the J-Link.
2. Start **J-Flash Lite** (V9.44).
3. Select device `R9A07G066M04GBG`, interface **JTAG**, speed **Auto**, then **OK**.
4. Program the files in this order. S-records contain their address. For `.bin` files, enter the address in **Prog. addr.**

| # | File | Address |
|---|---|---|
| 1 | `rza3m_ek_nor_ipl.srec` (IPL) | in file (`0x20000000`) |
| 2 | `boot/renesas/build/mode1/rza3m_mcuboot.srec` | in file (`0x20020000`) |
| 3 | Signed application for slot A, e.g. `Blinky_A.bin` | `0x20100000` |
| 4 | Signed application for slot B (optional) | `0x20200000` |

Signing the application is described in [guide 6](06-sign-and-flash-app.md). Before programming a slot, erase the whole slot (1 MiB), including its last 64 KiB sector that holds the trailer.

5. **Power-cycle** the board.
6. The console shows:

```text
[INF] RZ/A3M MCUboot - mode ram-load+revert
[INF] Primary   slot: ...
```

With no application programmed, MCUboot reports `no bootable image` and halts.

## 4.2 Experimental debug session

This launch configuration only attaches to the target. It does not download or program anything.

1. **Run → Debug Configurations → Renesas GDB Hardware Debugging → New**, name `RZA3M_MCUboot_release`.
2. **Main:** project `MCUboot`, application `boot/renesas/build/mode1/rza3m_mcuboot.elf`, build before launching disabled.
3. **Debugger → Debug hardware:** J-Link ARM, target `R9A07G066M04GBG`, board **EK-RZA3M NOR Boot**, JTAG, reset at connection **No**, reset after download **No**.
4. **Startup:** in *Load image and symbols*, keep only the MCUboot ELF with **Symbols only**. Uncheck **Set breakpoint at** and **Resume**.
5. **Initialization commands:**

```gdb
set breakpoint enable-memread 0
set breakpoint always-inserted off
delete breakpoints
```

Program the board with J-Flash Lite and power-cycle it first, then start the session. Example, reading the slot A trailer:

```gdb
x/8wx 0x201FFFE0
```

What works and what does not:

| Works | Does not work |
|---|---|
| Reading registers and memory | Programming NOR |
| Stepping through the code once | Resuming after a step (the session can disconnect) |
| | Debugging from reset (the debugger reset skips the Boot ROM → IPL → MCUboot sequence) |
