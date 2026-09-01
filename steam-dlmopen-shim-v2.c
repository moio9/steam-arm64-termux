#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

void *dlmopen(Lmid_t nsid, const char *filename, int flags)
{
    static void *(*real_dlmopen)(Lmid_t, const char *, int);
    const char *base;
    const char *replacement = NULL;

    if (!real_dlmopen)
        real_dlmopen = dlsym(RTLD_NEXT, "dlmopen");
    base = filename ? strrchr(filename, '/') : NULL;
    base = base ? base + 1 : filename;
    if (base && strcmp(base, "steamui.so") == 0)
        replacement = getenv("STEAMUI_PATCHED");
    else if (base && strcmp(base, "steamclient.so") == 0)
        replacement = getenv("STEAMCLIENT_PATCHED");
    else if (base && strcmp(base, "chromehtml.so") == 0)
        replacement = getenv("CHROMEHTML_PATCHED");
    if (replacement && *replacement)
        filename = replacement;
    return real_dlmopen(nsid, filename, flags);
}
