#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *fopen64(const char *path, const char *mode)
{
    static FILE *(*next_fn)(const char *, const char *);
    const char *bundle;
    if (!next_fn) next_fn = dlsym(RTLD_NEXT, "fopen64");
    if (path && (strcmp(path, "/etc/ssl/certs/ca-certificates.crt") == 0 ||
                 strcmp(path, "/etc/pki/tls/cert.pem") == 0 ||
                 strcmp(path, "/etc/ssl/ca-bundle.pem") == 0)) {
        bundle = getenv("SSL_CERT_FILE");
        if (bundle && *bundle) path = bundle;
    }
    return next_fn(path, mode);
}
