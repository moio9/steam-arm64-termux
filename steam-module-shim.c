#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

void *dlopen(const char *filename, int flags)
{
    static void *(*real_dlopen)(const char *, int);
    const char *base;
    const char *replacement = NULL;

    if (!real_dlopen)
        real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    base = filename ? strrchr(filename, '/') : NULL;
    base = base ? base + 1 : filename;
    if (base && strcmp(base, "steamclient.so") == 0)
        replacement = getenv("STEAMCLIENT_PATCHED");
    else if (base && strcmp(base, "chromehtml.so") == 0)
        replacement = getenv("CHROMEHTML_PATCHED");
    if (replacement && *replacement)
        filename = replacement;
    return real_dlopen(filename, flags);
}
