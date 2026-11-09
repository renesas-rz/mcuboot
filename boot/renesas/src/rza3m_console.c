/* RZ/A3M MCUboot port - minimal formatted logging over rza3m_plat_putc() (no libc).
 * Supports %% %c %s %d %i %u %x %X %p, length l/ll/z, width with optional 0 pad. */
#include <stdarg.h>
#include <stdint.h>
#include "rza3m_port.h"

static void out(char c) { if (c == '\n') { rza3m_plat_putc('\r'); } rza3m_plat_putc(c); }
static void out_str(const char *s) { while (*s) { out(*s++); } }

static void out_num(unsigned long long v, unsigned base, int upper, int neg, int width, char pad)
{
    char buf[24];
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do { buf[n++] = dig[v % base]; v /= base; } while (v != 0U && n < 22);
    if (neg) { width--; if (pad == '0') { out('-'); } }
    for (int w = n; w < width; w++) { out(pad); }
    if (neg && pad != '0') { out('-'); }
    while (n > 0) { out(buf[--n]); }
}

void rza3m_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') { out(*f); continue; }
        f++;
        char pad = ' ';
        int width = 0, lng = 0, left = 0;
        while (*f == '-' || *f == '0') {          /* flags, any order */
            if (*f == '-') { left = 1; } else { pad = '0'; }
            f++;
        }
        if (left) { pad = ' '; }                  /* '-' overrides '0' */
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f++ - '0'); }
        while (*f == 'l' || *f == 'z') { lng++; f++; }
        switch (*f) {
        case 'c': out((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == NULL) { s = "(null)"; }
            int n = 0;
            while (s[n] != '\0') { n++; }
            if (!left) { for (int w = n; w < width; w++) { out(' '); } }
            out_str(s);
            if (left)  { for (int w = n; w < width; w++) { out(' '); } }
        } break;
        case 'd': case 'i': {
            long long v = (lng >= 2) ? va_arg(ap, long long) : lng ? va_arg(ap, long) : va_arg(ap, int);
            out_num(v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v, 10U, 0, v < 0, width, pad);
        } break;
        case 'u': case 'x': case 'X': {
            unsigned long long v = (lng >= 2) ? va_arg(ap, unsigned long long)
                                 : lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_num(v, (*f == 'u') ? 10U : 16U, *f == 'X', 0, width, pad);
        } break;
        case 'p': out_str("0x"); out_num((uintptr_t)va_arg(ap, void *), 16U, 0, 0, 0, ' '); break;
        case '%': out('%'); break;
        case '\0': f--; break;
        default: out('%'); out(*f); break;
        }
    }
    va_end(ap);
}

void rza3m_assert_fail(const char *file, int line)
{
    rza3m_log("[ERR] assert %s:%d\n", file, line);
    for (;;) { __asm volatile("wfe"); }
}
