/* Shim for the most used TF-A <lib/utils_def.h> macros. Add others if a copied file needs them. */
#ifndef IPL_SHIM_UTILS_DEF_H
#define IPL_SHIM_UTILS_DEF_H
#ifndef U
#define U(x)  (x##U)
#define UL(x) (x##UL)
#define ULL(x) (x##ULL)
#endif
#define BIT_32(n) (U(1) << (n))
#define BIT_64(n) (ULL(1) << (n))
#define BIT(n)    BIT_64(n)
#define GENMASK_32(h, l) (((~U(0)) << (l)) & (~U(0) >> (32 - 1 - (h))))
#define GENMASK_64(h, l) (((~ULL(0)) << (l)) & (~ULL(0) >> (64 - 1 - (h))))
#define GENMASK(h, l)    GENMASK_64(h, l)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
