#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static const char *cert_path(const char *path)
{
    const char *bundle;
    if (!path) return path;
    if (strcmp(path, "/etc/ssl/certs/ca-certificates.crt") != 0 &&
        strcmp(path, "/etc/pki/tls/cert.pem") != 0 &&
        strcmp(path, "/etc/ssl/ca-bundle.pem") != 0)
        return path;
    bundle = getenv("SSL_CERT_FILE");
    return bundle && *bundle ? bundle : path;
}

int open(const char *path, int flags, ...)
{
    static int (*next_fn)(const char *, int, ...);
    mode_t mode = 0;
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "open");
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        return next_fn(cert_path(path), flags, mode);
    }
    return next_fn(cert_path(path), flags);
}

int open64(const char *path, int flags, ...)
{
    static int (*next_fn)(const char *, int, ...);
    mode_t mode = 0;
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "open64");
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        return next_fn(cert_path(path), flags, mode);
    }
    return next_fn(cert_path(path), flags);
}
