/* Shim for TF-A <arch_helpers.h>: barriers + cache maintenance used by IPL drivers.
 * If a copied file uses another helper, the compiler reports it - add it here. */
#ifndef IPL_SHIM_ARCH_HELPERS_H
#define IPL_SHIM_ARCH_HELPERS_H
#include <stdint.h>
#include <stddef.h>
#include "rza3m_port.h"
static inline void dsb(void)   { __asm volatile("dsb sy" ::: "memory"); }
static inline void dsbsy(void) { __asm volatile("dsb sy" ::: "memory"); }
static inline void dmb(void)   { __asm volatile("dmb sy" ::: "memory"); }
static inline void dmbsy(void) { __asm volatile("dmb sy" ::: "memory"); }
static inline void isb(void)   { __asm volatile("isb"    ::: "memory"); }
static inline void flush_dcache_range(uintptr_t a, size_t n) { rza3m_dcache_clean_inv_range(a, n); }
static inline void clean_dcache_range(uintptr_t a, size_t n) { rza3m_dcache_clean_inv_range(a, n); }
static inline void inv_dcache_range(uintptr_t a, size_t n)   { rza3m_dcache_clean_inv_range(a, n); }
static inline uint64_t read_cntpct_el0(void) { uint64_t v; __asm volatile("isb; mrs %0, cntpct_el0" : "=r"(v)); return v; }
static inline uint64_t read_cntfrq_el0(void) { uint64_t v; __asm volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v; }
#endif
