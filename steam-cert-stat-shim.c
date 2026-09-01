#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

int __lxstat(int ver, const char *path, struct stat *buf)
{
    static int (*next_fn)(int, const char *, struct stat *);
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "__lxstat");
    return next_fn(ver, cert_path(path), buf);
}

int __xstat(int ver, const char *path, struct stat *buf)
{
    static int (*next_fn)(int, const char *, struct stat *);
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "__xstat");
    return next_fn(ver, cert_path(path), buf);
}

int __lxstat64(int ver, const char *path, struct stat64 *buf)
{
    static int (*next_fn)(int, const char *, struct stat64 *);
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "__lxstat64");
    return next_fn(ver, cert_path(path), buf);
}

int __xstat64(int ver, const char *path, struct stat64 *buf)
{
    static int (*next_fn)(int, const char *, struct stat64 *);
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "__xstat64");
    return next_fn(ver, cert_path(path), buf);
}

char *realpath(const char *path, char *resolved)
{
    static char *(*next_fn)(const char *, char *);
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "realpath");
    return next_fn(cert_path(path), resolved);
}
