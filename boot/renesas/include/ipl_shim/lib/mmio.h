/* Shim for TF-A <lib/mmio.h> - lets the copied IPL drivers compile unchanged. */
#ifndef IPL_SHIM_MMIO_H
#define IPL_SHIM_MMIO_H
#include <stdint.h>
static inline void     mmio_write_8 (uintptr_t a, uint8_t  v) { *(volatile uint8_t  *)a = v; }
static inline uint8_t  mmio_read_8  (uintptr_t a)             { return *(volatile uint8_t  *)a; }
static inline void     mmio_write_16(uintptr_t a, uint16_t v) { *(volatile uint16_t *)a = v; }
static inline uint16_t mmio_read_16 (uintptr_t a)             { return *(volatile uint16_t *)a; }
static inline void     mmio_write_32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline uint32_t mmio_read_32 (uintptr_t a)             { return *(volatile uint32_t *)a; }
static inline void     mmio_write_64(uintptr_t a, uint64_t v) { *(volatile uint64_t *)a = v; }
static inline uint64_t mmio_read_64 (uintptr_t a)             { return *(volatile uint64_t *)a; }
static inline void mmio_clrbits_32(uintptr_t a, uint32_t c)             { mmio_write_32(a, mmio_read_32(a) & ~c); }
static inline void mmio_setbits_32(uintptr_t a, uint32_t s)             { mmio_write_32(a, mmio_read_32(a) | s); }
static inline void mmio_clrsetbits_32(uintptr_t a, uint32_t c, uint32_t s) { mmio_write_32(a, (mmio_read_32(a) & ~c) | s); }
#endif
