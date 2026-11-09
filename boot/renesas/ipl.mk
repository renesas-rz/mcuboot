# ---- replaces ipl_sources.mk: builds the COPIED IPL drivers in src/plat/ipl/ ----------------
# include right after the OBJS line in boot/rza3m/Makefile:   include ipl.mk
IPL_VSRCS := $(wildcard src/plat/ipl/*.c) src/plat/rza3m_plat_ipl.c
PORT_SRCS := $(filter-out src/plat/rza3m_plat_ipl.c,$(PORT_SRCS))
OBJS      := $(filter-out %rza3m_plat_ipl.c.o,$(OBJS)) $(patsubst %,$(BUILD)/obj/%.o,$(IPL_VSRCS))

# shims first so <lib/mmio.h>, <drivers/delay_timer.h>, <platform_def.h> resolve to them
CFLAGS += -Iinclude/ipl_shim -Isrc/plat/ipl

$(BUILD)/obj/src/plat/ipl/%.o: CFLAGS += -include ipl_prelude.h
