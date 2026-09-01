#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "raknet-recv-shim check failed at line %d: %s\n", \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

typedef uint64_t (*count_function)(void);
typedef unsigned long (*sleep_us_function)(void);

struct target_result {
    uint64_t elapsed;
    int status;
};

static uint64_t elapsed_nanoseconds(const struct timespec *start,
                                    const struct timespec *end) {
    uint64_t seconds = (uint64_t)(end->tv_sec - start->tv_sec);
    long nanoseconds = end->tv_nsec - start->tv_nsec;

    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000L;
    }
    return seconds * UINT64_C(1000000000) + (uint64_t)nanoseconds;
}

static void *run_target_thread(void *argument) {
    struct target_result *result = argument;
    struct timespec start;
    struct timespec end;
    int index;

    if (pthread_setname_np(pthread_self(), "Raknet-RecvFrom") != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        result->status = 1;
        return NULL;
    }
    for (index = 0; index < 4; index++) {
        if (sched_yield() != 0) {
            result->status = 1;
            return NULL;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        result->status = 1;
        return NULL;
    }
    result->elapsed = elapsed_nanoseconds(&start, &end);
    return NULL;
}

int main(int argc, char **argv) {
    count_function count_backoffs;
    sleep_us_function configured_sleep;
    struct target_result result = {0};
    pthread_t target_thread;
    uint64_t before;
    uint64_t after;
    int enabled;
    int index;
    void *symbol;

    CHECK(argc == 2);
    enabled = strcmp(argv[1], "enabled") == 0;
    CHECK(enabled || strcmp(argv[1], "disabled") == 0);
    _Static_assert(sizeof(count_backoffs) == sizeof(symbol),
                   "function and data pointers must have equal size");
    symbol = dlsym(RTLD_DEFAULT, "tgcompat_raknet_recv_backoff_count");
    memcpy(&count_backoffs, &symbol, sizeof(count_backoffs));
    symbol = dlsym(RTLD_DEFAULT, "tgcompat_raknet_recv_sleep_us");
    memcpy(&configured_sleep, &symbol, sizeof(configured_sleep));
    CHECK(count_backoffs != NULL);
    CHECK(configured_sleep != NULL);
    CHECK(configured_sleep() == (enabled ? 1000UL : 0UL));

    CHECK(pthread_setname_np(pthread_self(), "not-raknet") == 0);
    before = count_backoffs();
    for (index = 0; index < 16; index++) {
        CHECK(sched_yield() == 0);
    }
    CHECK(count_backoffs() == before);

    before = count_backoffs();
    CHECK(pthread_create(&target_thread, NULL, run_target_thread, &result) == 0);
    CHECK(pthread_join(target_thread, NULL) == 0);
    CHECK(result.status == 0);
    after = count_backoffs();
    if (enabled) {
        CHECK(after - before == 4);
        CHECK(result.elapsed >= UINT64_C(2000000));
    } else {
        CHECK(after == before);
    }

    puts("RakNet receive-thread backoff shim: PASS");
    return 0;
}
