/* Shim for <errno.h> (no libc). TF-A/IPL drivers return -Exxx codes. */
#ifndef IPL_SHIM_ERRNO_H
#define IPL_SHIM_ERRNO_H
#define EPERM    1
#define ENOENT   2
#define EIO      5
#define ENXIO    6
#define E2BIG    7
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define ENODEV  19
#define EINVAL  22
#define ENOSPC  28
#define ERANGE  34
#define ENOSYS  38
#define ENOTSUP 95
#define EOPNOTSUPP 95
#define ETIMEDOUT 110
#define EALREADY 114
#endif
