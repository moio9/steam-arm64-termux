#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int access(const char *path, int mode)
{
    static int (*next_access)(const char *, int);
    if (!next_access) next_access = dlsym(RTLD_NEXT, "access");
    return next_access(cert_path(path), mode);
}

FILE *fopen(const char *path, const char *mode)
{
    static FILE *(*next_fopen)(const char *, const char *);
    if (!next_fopen) next_fopen = dlsym(RTLD_NEXT, "fopen");
    return next_fopen(cert_path(path), mode);
}
