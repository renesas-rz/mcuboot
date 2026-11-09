/* Freestanding string functions (no libc). The port runs MMU-off, where DDR is Device
 * memory and unaligned accesses fault - so only aligned 64-bit accesses are used. */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *dp = d; const uint8_t *sp = s;
    if ((((uintptr_t)dp | (uintptr_t)sp) & 7U) == 0U) {
        while (n >= 8U) { *(uint64_t *)(void *)dp = *(const uint64_t *)(const void *)sp; dp += 8; sp += 8; n -= 8U; }
    }
    while (n--) { *dp++ = *sp++; }
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *dp = d; const uint8_t *sp = s;
    if (dp == sp || n == 0U) { return d; }
    if (dp < sp) { while (n--) { *dp++ = *sp++; } }
    else { dp += n; sp += n; while (n--) { *--dp = *--sp; } }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    uint8_t *dp = d;
    while (n--) { *dp++ = (uint8_t)c; }
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a, *y = b;
    for (; n; n--, x++, y++) { if (*x != *y) { return (int)*x - (int)*y; } }
    return 0;
}

size_t strlen(const char *s) { const char *p = s; while (*p) { p++; } return (size_t)(p - s); }

/* BSD strlcpy: copies at most size-1 chars, always NUL-terminates (if size > 0),
 * returns strlen(src). Needed by the vendored IPL qspiflash driver (flash_get_info). */
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size != 0U) {
        size_t n = (len >= size) ? (size - 1U) : len;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
