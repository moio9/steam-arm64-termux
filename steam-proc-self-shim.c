#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *virtual_target(const char *path)
{
    const char *target;

    if (path == NULL || strcmp(path, "/proc/self/exe") != 0)
        return NULL;
    target = getenv("TGCOMPAT_PROC_SELF_EXE");
    return target != NULL && target[0] == '/' ? target : NULL;
}

static ssize_t copy_target(const char *target, char *buffer, size_t size)
{
    size_t length;

    if (size == 0) {
        errno = EINVAL;
        return -1;
    }
    length = strlen(target);
    if (length > size)
        length = size;
    memcpy(buffer, target, length);
    return (ssize_t)length;
}

ssize_t readlink(const char *path, char *buffer, size_t size)
{
    static ssize_t (*next_readlink)(const char *, char *, size_t);
    const char *target = virtual_target(path);

    if (target != NULL)
        return copy_target(target, buffer, size);
    if (next_readlink == NULL)
        next_readlink = dlsym(RTLD_NEXT, "readlink");
    return next_readlink(path, buffer, size);
}

ssize_t readlinkat(int fd, const char *path, char *buffer, size_t size)
{
    static ssize_t (*next_readlinkat)(int, const char *, char *, size_t);
    const char *target = virtual_target(path);

    if (target != NULL)
        return copy_target(target, buffer, size);
    if (next_readlinkat == NULL)
        next_readlinkat = dlsym(RTLD_NEXT, "readlinkat");
    return next_readlinkat(fd, path, buffer, size);
}
