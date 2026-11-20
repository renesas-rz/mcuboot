#!/usr/bin/env bash
# sign_image.sh - sign an RZ/A3M application image for MCUboot.
#
# usage:
#   sign_image.sh <ramload|swap> <factory|test|raw> <in.elf|in.bin> <version> <out.bin> [slot_addr]
#
#   factory  --pad --confirm   permanent image; required for the first image in ram-load+revert
#   test     --pad             trial boot
#   raw      no trailer        OTA payload; the application calls rza3m_app_mark_pending()
#
#   slot_addr (optional)       also write <out>.srec at this NOR address
#                              slot A = 0x20100000, slot B = 0x20200000
#
# Optional environment:
#   MCUBOOT_DIR   MCUboot repository (default: derived from this script's location)
#   SIGNING_KEY   private key; only needed when MCUboot is built with MCUBOOT_SIGN_*
#   PYTHON        Python interpreter (default: python3, python, then py -3)
#   OBJCOPY       objcopy for .elf input and .srec output (default: aarch64-none-elf-objcopy)
#
# Layout values below must match include/rza3m_flash_layout.h.
set -euo pipefail

HDR_SIZE=0x1000        # RZA3M_IMG_HDR_SIZE
ALIGN=8                # RZA3M_NOR_WRITE_ALIGN
SLOT_SIZE=0x100000     # RZA3M_PART_SLOT_SIZE
ERASE_SIZE=0x10000     # RZA3M_NOR_ERASE_SIZE
LOAD_ADDR=0x401FF000   # RZA3M_APP_LOAD_ADDR
ENTRY_ADDR=$((LOAD_ADDR + HDR_SIZE))
FSP_HDR_SIZE=512       # RZ FSP loader header, "RZ FSP Application" at 0x1E0

die()   { echo "[sign] ERROR: $*" >&2; exit 1; }
usage() { sed -n '4,20p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }

# Accept Windows paths (C:\...) passed by e2 studio when running under Git Bash/MSYS.
to_unix() { if command -v cygpath >/dev/null 2>&1; then cygpath -u "$1"; else printf '%s\n' "$1"; fi; }

# Unsigned little-endian 64-bit value at a byte offset.
rd64() { od -An -tu8 -j "$2" -N8 "$1" | tr -d ' '; }

[[ $# -eq 5 || $# -eq 6 ]] || usage

MODE=$1
KIND=$2
IN=$(to_unix "$3")
VER=$4
OUT=$(to_unix "$5")
SLOT_ADDR=${6:-}

case "$MODE" in ramload|swap) ;; *) die "mode must be ramload or swap" ;; esac
case "$KIND" in factory|test|raw) ;; *) die "kind must be factory, test or raw" ;; esac
[[ "$VER" =~ ^[0-9]+(\.[0-9]+){0,2}(\+[0-9]+)?$ ]] || die "version must look like 1.2.3 or 1.2.3+4"
[[ -z "$SLOT_ADDR" || "$SLOT_ADDR" =~ ^(0[xX][[:xdigit:]]+|[0-9]+)$ ]] || die "slot_addr must be a NOR address"
[[ -f "$IN" ]] || die "input not found: $IN"
[[ "$(realpath "$IN")" != "$(realpath -m "$OUT")" ]] || die "input and output must differ"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
MCUBOOT_DIR=$(to_unix "${MCUBOOT_DIR:-$SCRIPT_DIR/../../..}")
IMGTOOL="$MCUBOOT_DIR/scripts/imgtool.py"
[[ -f "$IMGTOOL" ]] || die "imgtool not found: $IMGTOOL (set MCUBOOT_DIR)"

OBJCOPY=${OBJCOPY:-aarch64-none-elf-objcopy}

if [[ -n "${PYTHON:-}" ]]; then
    read -r -a PY <<< "$PYTHON"
elif command -v python3 >/dev/null 2>&1 && python3 -c '' >/dev/null 2>&1; then
    PY=(python3)
elif command -v python >/dev/null 2>&1; then
    PY=(python)
elif command -v py >/dev/null 2>&1; then
    PY=(py -3)
else
    die "Python not found (set PYTHON)"
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$(dirname "$OUT")"

# ---- 1. Raw binary (convert ELF here, so e2 studio's secondary-output order does not matter)
BIN=$IN
if [[ "$IN" == *.elf ]]; then
    command -v "$OBJCOPY" >/dev/null 2>&1 || die "$OBJCOPY not found (set OBJCOPY)"
    "$OBJCOPY" -O binary "$IN" "$WORK/app.bin" || die "objcopy failed"
    BIN="$WORK/app.bin"
fi

FILE_SIZE=$(wc -c < "$BIN")
(( FILE_SIZE <= 2 * SLOT_SIZE )) || die "binary is $FILE_SIZE bytes - check the section load addresses (LMA gap?)"

# ---- 2. Strip the RZ FSP loader header and check the link address
PAYLOAD="$WORK/payload.bin"
if [[ "$(dd if="$BIN" bs=1 skip=$((0x1E0)) count=18 status=none)" == "RZ FSP Application" ]]; then
    START=$(rd64 "$BIN" 0)
    COPY=$(rd64 "$BIN" 16)
    SIZE=$(rd64 "$BIN" 32)
    printf '[sign] FSP header: entry=0x%X copy=0x%X size=0x%X\n' "$START" "$COPY" "$SIZE"

    (( SIZE > 0 && SIZE <= FILE_SIZE - FSP_HDR_SIZE )) || die "FSP header size does not match the file"
    if [[ "$MODE" == ramload ]]; then
        (( START == ENTRY_ADDR )) || die "$(printf 'entry 0x%X, expected 0x%X - link the app at LOAD_ADDR+HDR_SIZE' "$START" "$ENTRY_ADDR")"
        (( COPY  == ENTRY_ADDR )) || die "$(printf 'copy address 0x%X, expected 0x%X' "$COPY" "$ENTRY_ADDR")"
    fi
    dd if="$BIN" of="$PAYLOAD" bs=64K iflag=skip_bytes,count_bytes skip="$FSP_HDR_SIZE" count="$SIZE" status=none
else
    [[ "$MODE" == ramload ]] && echo "[sign] WARNING: no FSP header - entry address not checked"
    cp -- "$BIN" "$PAYLOAD"
fi

# ---- 3. imgtool
ARGS=(sign
      --header-size "$HDR_SIZE" --pad-header
      --align "$ALIGN"
      --slot-size "$SLOT_SIZE"
      --max-sectors $((SLOT_SIZE / ERASE_SIZE))
      --version "$VER")

if [[ "$MODE" == ramload ]]; then
    ARGS+=(--load-addr "$LOAD_ADDR")
fi

if [[ -n "${SIGNING_KEY:-}" ]]; then
    ARGS+=(--key "$(to_unix "$SIGNING_KEY")")
fi

case "$KIND" in
    factory) ARGS+=(--pad --confirm) ;;
    test)    ARGS+=(--pad) ;;
    raw)     ;;
esac

echo "[sign] $MODE/$KIND v$VER -> $OUT"
"${PY[@]}" "$IMGTOOL" "${ARGS[@]}" "$PAYLOAD" "$OUT" || die "imgtool sign failed"

"${PY[@]}" "$IMGTOOL" dumpinfo "$OUT" > "$WORK/dump.txt" || die "imgtool dumpinfo failed"
sed -n '1,40p' "$WORK/dump.txt"

# ---- 4. Optional S-record at the slot address
if [[ -n "$SLOT_ADDR" ]]; then
    SREC="${OUT%.*}.srec"
    "$OBJCOPY" -I binary -O srec --srec-forceS3 --srec-len=32 \
        --change-addresses="$SLOT_ADDR" "$OUT" "$SREC" || die "srec conversion failed"
    echo "[sign] S-record: $SREC (at $SLOT_ADDR)"
fi

echo "[sign] SUCCESS"
