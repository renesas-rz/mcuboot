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
