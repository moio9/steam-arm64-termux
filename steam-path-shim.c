#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char *redirect_path(const char *path, char *buf, size_t size)
{
    const char *base;
    size_t base_len;

    if (!path || strncmp(path, "/tmp", 4) != 0 || (path[4] && path[4] != '/'))
        return path;

    base = getenv("STEAM_TMP");
    if (!base || !*base)
        return path;

    base_len = strlen(base);
    if (base_len + strlen(path + 4) + 1 > size) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(buf, base, base_len);
    strcpy(buf + base_len, path + 4);
    return buf;
}

#define RESOLVE(name) do { \
    if (!real_##name) real_##name = dlsym(RTLD_NEXT, #name); \
} while (0)

int open(const char *path, int flags, ...)
{
    static int (*real_open)(const char *, int, ...);
    char mapped[4096];
    mode_t mode = 0;
    RESOLVE(open);
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    path = redirect_path(path, mapped, sizeof(mapped));
    if (!path) return -1;
    return (flags & (O_CREAT | O_TMPFILE)) ? real_open(path, flags, mode)
                                          : real_open(path, flags);
}

int open64(const char *path, int flags, ...)
{
    static int (*real_open64)(const char *, int, ...);
    char mapped[4096];
    mode_t mode = 0;
    RESOLVE(open64);
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    path = redirect_path(path, mapped, sizeof(mapped));
    if (!path) return -1;
    return (flags & (O_CREAT | O_TMPFILE)) ? real_open64(path, flags, mode)
                                          : real_open64(path, flags);
}

FILE *fopen(const char *path, const char *mode)
{
    static FILE *(*real_fopen)(const char *, const char *);
    char mapped[4096];
    RESOLVE(fopen);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_fopen(path, mode) : NULL;
}

int mkdir(const char *path, mode_t mode)
{
    static int (*real_mkdir)(const char *, mode_t);
    char mapped[4096];
    RESOLVE(mkdir);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_mkdir(path, mode) : -1;
}

int access(const char *path, int mode)
{
    static int (*real_access)(const char *, int);
    char mapped[4096];
    RESOLVE(access);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_access(path, mode) : -1;
}

int stat(const char *path, struct stat *st)
{
    static int (*real_stat)(const char *, struct stat *);
    char mapped[4096];
    RESOLVE(stat);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_stat(path, st) : -1;
}

int lstat(const char *path, struct stat *st)
{
    static int (*real_lstat)(const char *, struct stat *);
    char mapped[4096];
    RESOLVE(lstat);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_lstat(path, st) : -1;
}

int chdir(const char *path)
{
    static int (*real_chdir)(const char *);
    char mapped[4096];
    RESOLVE(chdir);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_chdir(path) : -1;
}

int __xstat(int ver, const char *path, struct stat *st)
{
    static int (*real_xstat)(int, const char *, struct stat *);
    char mapped[4096];
    if (!real_xstat) real_xstat = dlsym(RTLD_NEXT, "__xstat");
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_xstat(ver, path, st) : -1;
}

int __lxstat(int ver, const char *path, struct stat *st)
{
    static int (*real_lxstat)(int, const char *, struct stat *);
    char mapped[4096];
    if (!real_lxstat) real_lxstat = dlsym(RTLD_NEXT, "__lxstat");
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_lxstat(ver, path, st) : -1;
}

int __xstat64(int ver, const char *path, struct stat64 *st)
{
    static int (*real_xstat64)(int, const char *, struct stat64 *);
    char mapped[4096];
    if (!real_xstat64) real_xstat64 = dlsym(RTLD_NEXT, "__xstat64");
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_xstat64(ver, path, st) : -1;
}

int __lxstat64(int ver, const char *path, struct stat64 *st)
{
    static int (*real_lxstat64)(int, const char *, struct stat64 *);
    char mapped[4096];
    if (!real_lxstat64) real_lxstat64 = dlsym(RTLD_NEXT, "__lxstat64");
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_lxstat64(ver, path, st) : -1;
}

int chmod(const char *path, mode_t mode)
{
    static int (*real_chmod)(const char *, mode_t);
    char mapped[4096];
    RESOLVE(chmod);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_chmod(path, mode) : -1;
}

int chown(const char *path, uid_t owner, gid_t group)
{
    static int (*real_chown)(const char *, uid_t, gid_t);
    char mapped[4096];
    RESOLVE(chown);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_chown(path, owner, group) : -1;
}

int lchown(const char *path, uid_t owner, gid_t group)
{
    static int (*real_lchown)(const char *, uid_t, gid_t);
    char mapped[4096];
    RESOLVE(lchown);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_lchown(path, owner, group) : -1;
}

DIR *opendir(const char *path)
{
    static DIR *(*real_opendir)(const char *);
    char mapped[4096];
    RESOLVE(opendir);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_opendir(path) : NULL;
}

int unlink(const char *path)
{
    static int (*real_unlink)(const char *);
    char mapped[4096];
    RESOLVE(unlink);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_unlink(path) : -1;
}

int rmdir(const char *path)
{
    static int (*real_rmdir)(const char *);
    char mapped[4096];
    RESOLVE(rmdir);
    path = redirect_path(path, mapped, sizeof(mapped));
    return path ? real_rmdir(path) : -1;
}

int rename(const char *oldpath, const char *newpath)
{
    static int (*real_rename)(const char *, const char *);
    char old_mapped[4096], new_mapped[4096];
    RESOLVE(rename);
    oldpath = redirect_path(oldpath, old_mapped, sizeof(old_mapped));
    newpath = redirect_path(newpath, new_mapped, sizeof(new_mapped));
    return oldpath && newpath ? real_rename(oldpath, newpath) : -1;
}
