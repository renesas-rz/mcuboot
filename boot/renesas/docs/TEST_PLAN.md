# On-target test plan — trial boot, confirmation, fallback (run per mode)

Log every run: MCUboot UART lines, `[APP]` lines, boot time (reset → `[APP]` banner).

| # | Setup | Action | Expected mode 1 (RAM-load + revert) | Swap modes (2/3) |
|---|-------|--------|-------------------------------------|------------------|
| T1 | A_factory only, B erased | power on | slot A v1.0.0 CONFIRMED | same |
| T2 | A_factory + B_good (test) | power on | slot B v1.1.0 IN TRIAL → `confirm rc=0` | swap, v1.1.0 from primary, confirms |
| T3 | after T2 | reset | slot B v1.1.0 CONFIRMED | no swap, v1.1.0 CONFIRMED |
| T4 | A_factory + B_bad | power on | slot B v1.2.0 IN TRIAL, no confirm | swap, trial |
| T5 | after T4 | reset | revert → slot A v1.0.0 | swap back → v1.0.0 |
| T6 | after T5 | reset | slot A again (no ping-pong) | v1.0.0, no swap |
| T7 | B_good with 1 byte flipped | power on | hash fails, B rejected, A boots | rejected, no swap |
| T8 | B lower version than A (test) | power on | not selected (highest version wins) — document | swap happens |
| T9 | B written raw, app calls `rza3m_app_mark_pending(false)`, reset | | trial boot of B | as T2 |
| T10 | B raw, no mark pending | power on | record behaviour — feasibility finding | no update |
| T11 | during T4 trial: power cut instead of reset | | as T5 | as T5 |
| T12 | power cut during swap, 20× random timing | | n/a | swap resumes/reverts, never unbootable (OR-002) |
| T13 | reset while the app is writing slot B | | slot A boots (OR-001) | primary untouched |
| T14 | both slots erased/corrupted | power on | `HALT: no bootable image` (FR-014) | same |
| T15 | Sign an image | inspect `imgtool dumpinfo` output | no custom UI-bank TLV | same |

### RAM-load+revert recovery checks

Observed on EK-RZ/A3M with the SHA-256-only build and J-Link resets:

| Starting state | Observed result |
|---|---|
| Confirmed A; newer B with a single payload bit flipped | B rejected; MCUboot handed off to A. |
| Confirmed A; newer B with corrupt trailer magic and a valid image hash | B erased; MCUboot handed off to A. |
| Confirmed A; B blank | B reported missing; MCUboot handed off to A. |
| Both slots blank | MCUboot logged `HALT: no bootable image`; no application handoff. |

To recover from the halt, program a known-good confirmed (`factory`) image
into an application slot and reset. Use an image signed by the trusted key
when signature verification is enabled. On the board, restoring both original
confirmed images with J-Link recovered normal boot into Blinky from slot B
without rewriting IPL or MCUboot. These checks did not interrupt a flash
operation or remove power.

### RAM-load+revert interrupted-update states

The following slot B states were deliberately staged and read back with
J-Link before resetting. Slot A was left untouched.

| Simulated interruption | Slot B state at reset | Observed result |
|---|---|---|
| Inactive-slot erase | First 64 KiB erased; old confirmed trailer intact | B reported missing; MCUboot handed off to A v1.0.0. |
| Inactive-slot programming | First 8 KiB programmed; remaining payload and trailer erased | B rejected and erased; MCUboot handed off to A v1.0.0. |
| Trailer programming | Complete payload; first half of trailer magic present, second half and `image_ok` erased | B rejected and erased; MCUboot handed off to A v1.0.0. |

After restoring confirmed B v1.0.4, Blinky started from B again. These
staged cases do not interrupt live NOR operations.

### RAM-load+revert live erase reset

A temporary test build armed the watchdog after the NOR reported
write-in-progress during erase of the second 64 KiB sector of slot B.
In two runs, the chip still reported `WIP=1` shortly before the watchdog
reset, and startup went to `SCIF Download mode due to error` before the IPL
banner. This suggests the boot ROM could not read NOR while the erase was
still busy; the UART evidence does not establish the exact ROM failure cause.

On the repeat, a second J-Link reset after a three-second wait recovered
without flash writes: IPL and MCUboot started, slot B was missing, and
MCUboot handed off to confirmed slot A v1.0.0. Slot A and IPL were not
reprogrammed. The original bootloader and slot B were then restored and
Blinky booted again.

For OR-001 and OR-002, this demonstrates preservation of the confirmed image
in the tested live erase case, but not automatic recovery on the first reset.
Live page programming, trailer flag writes and actual power cuts remain untested.

### RAM-load+revert watchdog checks

- With `HEALTH_FORCE_FAIL`, press reset once to start a fresh trial. Check
  that the inherited watchdog counter advances, the app does not confirm,
  and the board reboots without another reset. The next boot should log
  `Previous reset: watchdog`, erase the failed trial, and boot the confirmed slot.
- With `APP_TEST_WATCHDOG_REFRESH`, check that the trial confirms and reports
  20 seconds of watchdog refresh without rebooting. On the next boot the same
  image should start as confirmed, without arming the trial watchdog.

Measure per mode: MCUboot start → app banner time, swap duration for a full slot, erase cycles per
update, MCUboot binary size and DDR footprint.

Known MCUboot behaviours (document, not bugs): single trial boot (no attempt counter);
RAM-load picks the highest version; first image in RAM-load+revert must be signed `--confirm`.
