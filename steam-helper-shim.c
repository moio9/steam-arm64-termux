#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *replace_once(const char *input, const char *from, const char *to)
{
    const char *at;
    char *out;
    size_t before, size;

    if (!input || !from || !to || !(at = strstr(input, from)))
        return input ? strdup(input) : NULL;
    before = (size_t)(at - input);
    size = before + strlen(to) + strlen(at + strlen(from)) + 1;
    out = malloc(size);
    if (!out)
        return NULL;
    memcpy(out, input, before);
    strcpy(out + before, to);
    strcat(out, at + strlen(from));
    return out;
}

static char *map_command(const char *command)
{
    const char *lsof_helper;
    char *first, *second, *third;
    first = replace_once(command, "/steamsysinfo", "/steamsysinfo-patched");
    if (!first)
        return NULL;
    second = replace_once(first, "/steamwebhelper.sh", "/steamwebhelper-patched.sh");
    free(first);
    if (!second)
        return NULL;
    lsof_helper = getenv("STEAM_ARM64_LSOF");
    if (!lsof_helper || lsof_helper[0] != '/')
        return second;
    third = replace_once(second, "/bin/lsof", lsof_helper);
    free(second);
    return third;
}

int system(const char *command)
{
    static int (*real_system)(const char *);
    char *mapped;
    int result;
    if (!real_system)
        real_system = dlsym(RTLD_NEXT, "system");
    if (!command)
        return real_system(NULL);
    mapped = map_command(command);
    result = real_system(mapped ? mapped : command);
    free(mapped);
    return result;
}

FILE *popen(const char *command, const char *type)
{
    static FILE *(*real_popen)(const char *, const char *);
    char *mapped;
    FILE *result;
    if (!real_popen)
        real_popen = dlsym(RTLD_NEXT, "popen");
    mapped = map_command(command);
    result = real_popen(mapped ? mapped : command, type);
    free(mapped);
    return result;
}

FILE *popen64(const char *command, const char *type)
{
    static FILE *(*real_popen64)(const char *, const char *);
    char *mapped;
    FILE *result;
    if (!real_popen64)
        real_popen64 = dlsym(RTLD_NEXT, "popen64");
    mapped = map_command(command);
    result = real_popen64(mapped ? mapped : command, type);
    free(mapped);
    return result;
}
