# RZ/A3M MCUboot - standalone bare-metal build (no FSP). Place at <mcuboot>/boot/rza3m/.
#   make LAYOUT_OK=1 MODE=1 all        -> build/release/rza3m_mcuboot.bin
#   make LAYOUT_OK=1 MODE=1 LOG=4      (debug logging, incl. IPL driver INFO)
# Use the same aarch64 bare-metal GCC you use to build the IPL.

CROSS_COMPILE ?= aarch64-none-elf-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size

MODE    ?= 1
LOG     ?= 3
MCUBOOT := ../..
ifeq ($(MODE),1)
BUILD   := build/release
else
BUILD   := build/mode$(MODE)
endif

# ---- Debug / release selection (appended last so it wins) ----
DEBUG ?= 0
ifeq ($(DEBUG),1)
OPT_FLAGS := -O0 -g3 -ggdb -fno-omit-frame-pointer -fno-optimize-sibling-calls
ifeq ($(MODE),1)
BUILD     := build/debug
else
BUILD     := $(BUILD)_dbg
endif
else
OPT_FLAGS := -Os -g
endif

VERIFY_KEY ?=
ifneq ($(strip $(VERIFY_KEY)),)
BUILD := $(BUILD)_ec256
PYTHON ?= python
endif

PORT_SRCS := src/start.S src/rza3m_jump.S src/boot_main.c src/flash_map_backend.c \
             src/rza3m_console.c src/rza3m_string.c src/rza3m_cache.c src/rza3m_wdt.c
# All bootutil sources except the PSA crypto backends (*_psa.c): they need
# mbedTLS/PSA headers and aren't disabled by mcuboot_config.h.
# Add more files with BOOTUTIL_EXCLUDE="a.c b.c"
BOOTUTIL_EXCLUDE ?=
BOOTUTIL_DIR  := $(MCUBOOT)/boot/bootutil/src
BOOTUTIL_SRCS := $(filter-out $(BOOTUTIL_DIR)/%_psa.c $(addprefix $(BOOTUTIL_DIR)/,$(BOOTUTIL_EXCLUDE)), \
                 $(wildcard $(BOOTUTIL_DIR)/*.c))

list-bootutil:
	@printf '%s\n' $(BOOTUTIL_SRCS)
TC_SRCS := $(MCUBOOT)/ext/tinycrypt/lib/source/sha256.c $(MCUBOOT)/ext/tinycrypt/lib/source/utils.c

INCS := -Iinclude -I$(MCUBOOT)/boot/bootutil/include -I$(MCUBOOT)/ext/tinycrypt/lib/include

DEFS := -DRZA3M_MCUBOOT_MODE=$(MODE) -DMCUBOOT_LOG_LEVEL=$(LOG)
ifeq ($(LAYOUT_OK),1)
DEFS += -DRZA3M_LAYOUT_CONFIRMED
endif
ifneq ($(strip $(VERIFY_KEY)),)
ASN1_DIR := $(MCUBOOT)/ext/mbedtls-asn1
DEFS += -DMCUBOOT_SIGN_EC256
INCS += -I$(BUILD) -I$(ASN1_DIR)/include
PORT_SRCS += src/rza3m_verify_key.c
TC_SRCS += $(addprefix $(MCUBOOT)/ext/tinycrypt/lib/source/,ecc.c ecc_dsa.c)
ASN1_SRCS := $(addprefix $(ASN1_DIR)/src/,asn1parse.c platform_util.c)
endif

# -mstrict-align: runs MMU-off (DDR = Device memory, no unaligned access)
# -mgeneral-regs-only: no FP/SIMD, independent of CPTR_EL3 state left by the IPL
CFLAGS += -mcpu=cortex-a55 -mstrict-align -mgeneral-regs-only -ffreestanding -fno-common \
          -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wno-unused-parameter $(DEFS) $(INCS)
ASFLAGS := -mcpu=cortex-a55 $(DEFS) $(INCS)
LDFLAGS := -nostdlib -nostartfiles -static -Wl,--gc-sections -Wl,-Map=$(BUILD)/rza3m_mcuboot.map

SRCS := $(PORT_SRCS) $(BOOTUTIL_SRCS) $(TC_SRCS) $(ASN1_SRCS)
OBJS := $(patsubst %,$(BUILD)/obj/%.o,$(subst ../,,$(SRCS)))

CFLAGS  += $(OPT_FLAGS) -MMD -MP
ASFLAGS += $(OPT_FLAGS) -MMD -MP

include ipl.mk

MCUBOOT_ELF   := $(BUILD)/rza3m_mcuboot.elf
MCUBOOT_BIN   := $(BUILD)/rza3m_mcuboot.bin
MCUBOOT_SREC  := $(BUILD)/rza3m_mcuboot.srec

# Signed application -> SREC at primary slot base (memory-mapped NOR).
# Must match the primary slot in include/rza3m_flash_layout.h.
APP_SLOT0_ADDR  ?= 0x20100000
APP_SIGNED_BIN  ?= blinky_signed.bin
APP_SIGNED_SREC ?= $(BUILD)/blinky_signed.srec

all: $(MCUBOOT_BIN) $(MCUBOOT_SREC)

srec: $(MCUBOOT_SREC)
app-srec: $(APP_SIGNED_SREC)

$(BUILD)/obj/%.c.o: %.c
	@mkdir -p $(dir $@); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/obj/%.S.o: %.S
	@mkdir -p $(dir $@); $(CC) $(ASFLAGS) -c $< -o $@
$(BUILD)/obj/%.c.o: ../../%.c
	@mkdir -p $(dir $@); $(CC) $(CFLAGS) -c $< -o $@

ifneq ($(strip $(VERIFY_KEY)),)
$(BUILD)/obj/src/rza3m_verify_key.c.o: $(BUILD)/verify_key.h

$(BUILD)/verify_key.h: FORCE
	@mkdir -p $(dir $@)
	"$(PYTHON)" "$(MCUBOOT)/scripts/imgtool.py" getpub -k "$(VERIFY_KEY)" -o "$@.tmp"
	@mv "$@.tmp" "$@"

.PHONY: FORCE
FORCE:
endif

$(BUILD)/linker.ld: ld/rza3m_mcuboot.ld.S include/rza3m_flash_layout.h
	@mkdir -p $(BUILD); $(CC) -E -P -x c -DLINKER_SCRIPT $(DEFS) $(INCS) $< -o $@

$(MCUBOOT_ELF): $(OBJS) $(BUILD)/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T $(BUILD)/linker.ld $(OBJS) -lgcc -o $@
	$(SIZE) $@

$(MCUBOOT_BIN): $(MCUBOOT_ELF)
	$(OBJCOPY) -O binary $< $@
	@ls -l $@

# Same as FSP/IPL: ELF -> SREC, addresses come from the ELF LMAs (0x20020000...)
$(MCUBOOT_SREC): $(MCUBOOT_ELF)
	$(OBJCOPY) -O srec --srec-forceS3 --srec-len=32 $< $@
	@echo "First data record (must start at 20020000):"
	@grep -m1 '^S3' $@

# Raw signed image -> SREC relocated to the primary slot
$(APP_SIGNED_SREC): $(APP_SIGNED_BIN)
	@mkdir -p $(BUILD)
	$(OBJCOPY) -I binary -O srec --srec-forceS3 --srec-len=32 \
	    --change-addresses=$(APP_SLOT0_ADDR) $< $@
	@echo "First data record (must start at $(APP_SLOT0_ADDR)):"
	@grep -m1 '^S3' $@

-include $(OBJS:.o=.d)

clean:
	rm -rf build

.PHONY: all clean srec app-srec list-bootutil
