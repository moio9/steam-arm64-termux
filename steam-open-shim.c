#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static const char *mapped_path(const char *path, char *buf, size_t size)
{
    const char *base;
    size_t base_len;
    if (!path) return path;
    if (strcmp(path, "/etc/ssl/certs/ca-certificates.crt") == 0 ||
        strcmp(path, "/etc/pki/tls/cert.pem") == 0 ||
        strcmp(path, "/etc/ssl/ca-bundle.pem") == 0) {
        base = getenv("SSL_CERT_FILE");
        return base && *base ? base : path;
    }
    if (strncmp(path, "/tmp", 4) != 0 || (path[4] && path[4] != '/'))
        return path;
    base = getenv("STEAM_TMP");
    if (!base || !*base) return path;
    base_len = strlen(base);
    if (base_len + strlen(path + 4) + 1 > size) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(buf, base, base_len);
    strcpy(buf + base_len, path + 4);
    return buf;
}

int open(const char *path, int flags, ...)
{
    static int (*next_fn)(const char *, int, ...);
    char buf[4096];
    mode_t mode = 0;
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "open");
    path = mapped_path(path, buf, sizeof(buf));
    if (!path) return -1;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        return next_fn(path, flags, mode);
    }
    return next_fn(path, flags);
}

int open64(const char *path, int flags, ...)
{
    static int (*next_fn)(const char *, int, ...);
    char buf[4096];
    mode_t mode = 0;
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "open64");
    path = mapped_path(path, buf, sizeof(buf));
    if (!path) return -1;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        return next_fn(path, flags, mode);
    }
    return next_fn(path, flags);
}
