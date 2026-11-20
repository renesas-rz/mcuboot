# 8. Known limitations

| Area | Limitation |
|---|---|
| NOR write from MCUboot | Working. |
| NOR erase from MCUboot | **Partially working.** Repeated 64 KiB erases can stall, so removing a failed trial image (revert) is not reliable. Erase works from the application with the FSP `r_spibsc` driver. |
| Programming from e² studio | **Not working.** Use J-Flash Lite (V9.44) to program the S-records and the signed binaries. |
| Debugging | **Experimental.** A session can only read registers and memory and step through the code once. The debugger reset does not run the Boot ROM → IPL → MCUboot sequence. |
| Authenticity | The default build checks integrity only. For ECDSA-P256 verification, build with `VERIFY_KEY` and sign with the corresponding private key. See [guide 6](06-sign-and-flash-app.md). |
| Boot modes | Only `MODE=1` (RAM-load with revert) is tested. |
| Images | Single image (one application) only. |
| Signing | `sign_image` produces a signed `.bin` and is run manually. |
| Software reset | Not implemented in the example application. Reset manually after `mark_pending`. |
