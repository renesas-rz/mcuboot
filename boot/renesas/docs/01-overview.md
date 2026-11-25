# 1. Overview

## Boot flow

1. The RZ/A3M Boot ROM loads the **IPL** from QSPI NOR.
2. The IPL initialises DDR, reads the MCUboot application header at NOR `0x20020000`, then copies the payload from NOR `0x20020200` to DDR `0x40001000` and branches to `0x40001000`.
3. MCUboot checks both application slots, validates the selected image (SHA-256 and, if enabled, ECDSA-P256), copies it to DDR and jumps to it.
4. The application can query and confirm its boot state through the MCUboot application API.

The IPL is not modified. MCUboot replaces the application in the IPL's view.

## Memory map

| Region | NOR offset | Address | Size |
|---|---|---|---|
| IPL | `0x00000000` | `0x20000000` | — |
| MCUboot | `0x00020000` | `0x20020000` | `0x00080000` |
| Slot A (primary) | `0x00100000` | `0x20100000` | `0x00100000` |
| Slot B (secondary) | `0x00200000` | `0x20200000` | `0x00100000` |
| Scratch (swap-scratch only) | `0x00E00000` | `0x20E00000` | `0x00040000` |

| DDR region | Start | End (exclusive) |
|---|---|---|
| MCUboot reservation (code, data, BSS and stacks) | `0x40001000` | `0x401F1000` |
| Application image header | `0x401FF000` | `0x40200000` |
| Application runtime window (entry at start) | `0x40200000` | `0x407FF000` |
| Boot-info page | `0x407FF000` | `0x40800000` |

The authoritative values are in [`include/rza3m_flash_layout.h`](../include/rza3m_flash_layout.h).

## AArch64 handoff

- **IPL → MCUboot:** The application header specifies the DDR copy address, entry, and payload size. MCUboot expects AArch64 EL3 with DDR and the NOR read window initialized. It does not assume the IPL's MMU/cache state: startup masks DAIF, performs cache and TLB maintenance, runs with the MMU and D-cache off and the I-cache on, installs its exception vectors and stack, and clears BSS.
- **MCUboot → application:** MCUboot writes the boot-info block and branches at the same exception level (EL3 on the stock IPL path). DAIF remains masked. The handoff cleans and invalidates the D-cache to the point of coherency, disables the MMU and both caches, and invalidates the I-cache and relevant TLB entries before branching. It is not a hardware reset: the application must install its own stack and vectors, initialize its runtime and interrupt controller as needed, and configure its own MMU/cache state before enabling interrupts. It reads boot info at the fixed address; no register arguments are defined.
- **Trial boot:** The watchdog can already be running when the application starts. Keep its clocks enabled through startup and do not reopen or refresh it before the application has passed its health check and confirmed the trial. See [guide 5](05-application.md).

The Blinky reference build used for watchdog validation enters at
`0x40200000`. Its main DDR sections, stack and heap end at `0x402118F0`
(exclusive), below the boot-info page. It also uses on-chip SRAM for startup
code and MMU tables, and has an uncached BSS section at
`[0x80000020, 0x802118F0)` in its application address map. The image window
above is not a complete platform memory map.

Blinky is the reference application for the PoC handoff. Its board runs
demonstrate application startup, confirmation and watchdog rollback, not
FreeRTOS scheduler startup. If a separate FreeRTOS application is required,
verify all of its linker regions against the platform map, exclude the
boot-info page from allocations and startup clearing, and validate its
startup and trial lifecycle against the handoff above.

## Image format

| Item | Value |
|---|---|
| Image header size | `0x1000` |
| Write alignment | 8 |
| Erase sector | 64 KiB |
| Validation | SHA-256 (no signature by default) |

## Boot modes

Selected with `MODE=` when building MCUboot.

| `MODE` | Mode | Status |
|---|---|---|
| `1` | RAM-load with revert | **Default, tested** |
| other | swap / overwrite variants | Not validated |

In RAM-load mode MCUboot boots the **highest valid version** from either slot. A new image must therefore have a higher version than the one currently running.
