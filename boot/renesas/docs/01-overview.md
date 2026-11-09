# 1. Overview

## Boot flow

1. The RZ/A3M Boot ROM loads the **IPL** from QSPI NOR.
2. The IPL initialises DDR, copies **MCUboot** from NOR `0x20020000` to DDR `0x40000000` and jumps to it.
3. MCUboot checks both application slots, validates the selected image (SHA-256), copies it to DDR and jumps to it.
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

| DDR | Address |
|---|---|
| MCUboot runtime | `0x40000000` (≤ `0x001F0000`) |
| Application image header | `0x401FF000` |
| Application entry | `0x40200000` |
| Boot-info block | `0x407FF000` |

The authoritative values are in [`include/rza3m_flash_layout.h`](../include/rza3m_flash_layout.h).

## Image format

| Item | Value |
|---|---|
| Image header size | `0x1000` |
| Write alignment | 8 |
| Erase sector | 64 KiB |
| Custom TLV `0xA0` | UI bank number (ASCII decimal) |
| Validation | SHA-256 (no signature by default) |

## Boot modes

Selected with `MODE=` when building MCUboot.

| `MODE` | Mode | Status |
|---|---|---|
| `1` | RAM-load with revert | **Default, tested** |
| other | swap / overwrite variants | Not validated |

In RAM-load mode MCUboot boots the **highest valid version** from either slot. A new image must therefore have a higher version than the one currently running.
