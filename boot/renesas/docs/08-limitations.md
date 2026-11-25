# 8. Known limitations

| Area | Limitation |
|---|---|
| NOR erase from MCUboot | Failed-trial erase and fallback have succeeded on the board. Repeated update cycles and interruption recovery are not fully validated; a reset during live erase entered SCIF download mode before a later reset recovered. See the [test plan](TEST_PLAN.md). |
| Application NOR backend | The backend does not invalidate the controller and CPU caches after writes or erases. The FSP SPIBSC example requires these invalidations before reading updated flash; cached reads may be stale. |
| Authenticity | The default build checks integrity only. For ECDSA-P256 verification, build with `VERIFY_KEY` and sign with the corresponding private key. See [guide 6](06-sign-and-flash-app.md). |
| Boot modes | Only `MODE=1` (RAM-load with revert) is tested. |
| Images | Single image (one application) only. |
| Image packaging | `sign_image` is run manually. A cryptographic signature is added only when `SIGNING_KEY` is set. |
| Software reset | `rza3m_app_mark_pending()` does not reset the board. The application must arrange a reset; the Blinky reference uses a watchdog for its application-requested reboot. Do not reopen an inherited trial watchdog. |
