#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include "temp-path.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "flock-shim check failed at line %d: %s\n", \
            __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void expect_child_lock_result(const char *path, int expected_success) {
    pid_t child = fork();
    int status;

    CHECK(child >= 0);
    if (child == 0) {
        int descriptor = open(path, O_RDWR | O_CLOEXEC);
        int result;

        if (descriptor < 0) {
            _exit(90);
        }
        errno = 0;
        result = flock(descriptor, LOCK_EX | LOCK_NB);
        if (expected_success != 0) {
            _exit(result == 0 ? 0 : 91);
        }
        _exit(result < 0 && (errno == EACCES || errno == EAGAIN) ? 0 : 92);
    }
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

int main(void) {
    char path[TGC_TEST_PATH_CAPACITY];
    int descriptor;

    CHECK(tgc_test_temp_template(path, sizeof(path), "tgcompat-flock") == 0);
    descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(flock(descriptor, LOCK_EX | LOCK_NB) == 0);
    expect_child_lock_result(path, 0);
    CHECK(flock(descriptor, LOCK_UN) == 0);
    expect_child_lock_result(path, 1);

    errno = 0;
    CHECK(flock(descriptor, LOCK_SH | LOCK_EX) == -1);
    CHECK(errno == EINVAL);

    CHECK(close(descriptor) == 0);
    CHECK(unlink(path) == 0);
    puts("flock ENOSYS fcntl fallback: PASS");
    return EXIT_SUCCESS;
}
