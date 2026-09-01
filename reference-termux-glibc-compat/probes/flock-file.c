#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int descriptor;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    descriptor = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    if (flock(descriptor, LOCK_SH | LOCK_NB) != 0) {
        fprintf(stderr, "flock %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    if (flock(descriptor, LOCK_UN) != 0) {
        fprintf(stderr, "unlock %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    if (close(descriptor) != 0) {
        fprintf(stderr, "close %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    puts("removable-library shared lock: PASS");
    return EXIT_SUCCESS;
}
