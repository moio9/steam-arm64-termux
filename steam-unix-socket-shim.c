#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int is_update_ui_child(void)
{
    char buffer[4096];
    ssize_t count;
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);

    if (fd < 0)
        return 0;
    count = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (count <= 0)
        return 0;
    for (ssize_t i = 0; i + 16 <= count; ++i) {
        if (memcmp(buffer + i, "-child-update-ui", 16) == 0)
            return 1;
    }
    return 0;
}

static int rewrite_address(const struct sockaddr *address, socklen_t length,
        struct sockaddr_un *mapped, socklen_t *mapped_length)
{
    static const char x11_prefix[] = "/tmp/.X11-unix/";
    const struct sockaddr_un *unix_address;
    const char *tmp_root;
    size_t root_length;
    size_t suffix_length;
    size_t address_path_length;

    if (address == NULL || address->sa_family != AF_UNIX ||
            length <= offsetof(struct sockaddr_un, sun_path))
        return 0;
    unix_address = (const struct sockaddr_un *)address;
    if (unix_address->sun_path[0] == '\0') {
        address_path_length = (size_t)length -
            offsetof(struct sockaddr_un, sun_path) - 1;
        if (is_update_ui_child() &&
                ((address_path_length >= sizeof(x11_prefix) - 1 &&
                  memcmp(unix_address->sun_path + 1, x11_prefix,
                      sizeof(x11_prefix) - 1) == 0) ||
                 ((tmp_root = getenv("STEAM_TMP")) != NULL &&
                  (root_length = strlen(tmp_root)) > 0 &&
                  address_path_length >= root_length +
                      sizeof("/.X11-unix/") - 1 &&
                  memcmp(unix_address->sun_path + 1, tmp_root,
                      root_length) == 0 &&
                  memcmp(unix_address->sun_path + 1 + root_length,
                      "/.X11-unix/", sizeof("/.X11-unix/") - 1) == 0))) {
                errno = ECONNREFUSED;
                return -1;
        }
        return 0;
    }
    if ((strcmp(unix_address->sun_path, "/tmp") != 0 &&
             strncmp(unix_address->sun_path, "/tmp/", 5) != 0))
        return 0;
    if (strncmp(unix_address->sun_path, x11_prefix,
            sizeof(x11_prefix) - 1) == 0 && is_update_ui_child()) {
        errno = ECONNREFUSED;
        return -1;
    }

    tmp_root = getenv("STEAM_TMP");
    if (tmp_root == NULL || tmp_root[0] != '/')
        return 0;
    root_length = strlen(tmp_root);
    suffix_length = strlen(unix_address->sun_path + 4);
    if (root_length + suffix_length >= sizeof(mapped->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memset(mapped, 0, sizeof(*mapped));
    mapped->sun_family = AF_UNIX;
    memcpy(mapped->sun_path, tmp_root, root_length);
    memcpy(mapped->sun_path + root_length, unix_address->sun_path + 4,
        suffix_length + 1);
    *mapped_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
        root_length + suffix_length + 1);
    return 1;
}

#define DEFINE_SOCKET_CALL(name)                                             \
    int name(int fd, const struct sockaddr *address, socklen_t length)       \
    {                                                                        \
        static int (*next)(int, const struct sockaddr *, socklen_t);          \
        struct sockaddr_un mapped;                                           \
        socklen_t mapped_length;                                             \
        int result;                                                          \
        if (next == NULL)                                                    \
            next = dlsym(RTLD_NEXT, #name);                                  \
        result = rewrite_address(address, length, &mapped, &mapped_length);   \
        if (result < 0)                                                      \
            return -1;                                                       \
        if (result > 0)                                                      \
            return next(fd, (const struct sockaddr *)&mapped, mapped_length);\
        return next(fd, address, length);                                    \
    }

DEFINE_SOCKET_CALL(connect)
DEFINE_SOCKET_CALL(bind)
