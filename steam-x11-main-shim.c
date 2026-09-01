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

int connect(int fd, const struct sockaddr *address, socklen_t length)
{
    static int (*next_connect)(int, const struct sockaddr *, socklen_t);
    static const char x11_prefix[] = "/tmp/.X11-unix/";
    struct sockaddr_un mapped;
    const struct sockaddr_un *unix_address;
    const char *tmp_root;
    size_t root_length;
    size_t suffix_length;

    if (next_connect == NULL)
        next_connect = dlsym(RTLD_NEXT, "connect");
    if (address == NULL || address->sa_family != AF_UNIX ||
            length <= offsetof(struct sockaddr_un, sun_path))
        return next_connect(fd, address, length);

    unix_address = (const struct sockaddr_un *)address;
    if (unix_address->sun_path[0] == '\0' ||
            strncmp(unix_address->sun_path, x11_prefix,
                sizeof(x11_prefix) - 1) != 0)
        return next_connect(fd, address, length);
    if (is_update_ui_child()) {
        errno = ECONNREFUSED;
        return -1;
    }

    tmp_root = getenv("STEAM_TMP");
    if (tmp_root == NULL || tmp_root[0] != '/')
        return next_connect(fd, address, length);
    root_length = strlen(tmp_root);
    suffix_length = strlen(unix_address->sun_path + 4);
    if (root_length + suffix_length >= sizeof(mapped.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memset(&mapped, 0, sizeof(mapped));
    mapped.sun_family = AF_UNIX;
    memcpy(mapped.sun_path, tmp_root, root_length);
    memcpy(mapped.sun_path + root_length, unix_address->sun_path + 4,
        suffix_length + 1);
    return next_connect(fd, (const struct sockaddr *)&mapped,
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
            root_length + suffix_length + 1));
}
