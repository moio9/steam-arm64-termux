#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define TGCOMPAT_RAKNET_THREAD_NAME "Raknet-RecvFrom"
#define TGCOMPAT_THREAD_NAME_SIZE 16
#define TGCOMPAT_NAME_RECHECK_MASK 1023U
#define TGCOMPAT_MAX_SLEEP_US 10000UL

typedef int (*sched_yield_function)(void);

static sched_yield_function real_sched_yield;
static unsigned long configured_sleep_us;
static int diagnostics_enabled;
static int shim_initialized;
static _Atomic uint64_t backoff_count;

static _Thread_local unsigned int name_probe_counter;
static _Thread_local unsigned char thread_match_state;

static unsigned long parse_sleep_us(const char *value) {
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0' || strcmp(value, "0") == 0) {
        return 0;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0 ||
        parsed > TGCOMPAT_MAX_SLEEP_US) {
        return 0;
    }
    return parsed;
}

__attribute__((constructor)) static void initialize_raknet_recv_shim(void) {
    const char *diagnostics = getenv("TGCOMPAT_RAKNET_RECV_DIAGNOSTICS");
    void *symbol = dlsym(RTLD_NEXT, "sched_yield");

    _Static_assert(sizeof(real_sched_yield) == sizeof(symbol),
                   "function and data pointers must have equal size");
    memcpy(&real_sched_yield, &symbol, sizeof(real_sched_yield));
    configured_sleep_us =
        parse_sleep_us(getenv("TGCOMPAT_RAKNET_RECV_SLEEP_US"));
    diagnostics_enabled = diagnostics != NULL && strcmp(diagnostics, "1") == 0;
    shim_initialized = 1;
}

static int call_real_sched_yield(void) {
    if (real_sched_yield != NULL) {
        return real_sched_yield();
    }
#ifdef SYS_sched_yield
    return (int)syscall(SYS_sched_yield);
#else
    errno = ENOSYS;
    return -1;
#endif
}

static int current_thread_is_raknet_recv(void) {
    char name[TGCOMPAT_THREAD_NAME_SIZE];

    if (thread_match_state == 1) {
        return 1;
    }
    name_probe_counter++;
    if (thread_match_state == 2 &&
        (name_probe_counter & TGCOMPAT_NAME_RECHECK_MASK) != 0) {
        return 0;
    }
    if (pthread_getname_np(pthread_self(), name, sizeof(name)) == 0 &&
        strcmp(name, TGCOMPAT_RAKNET_THREAD_NAME) == 0) {
        thread_match_state = 1;
        return 1;
    }
    thread_match_state = 2;
    return 0;
}

static int sleep_for_configured_backoff(void) {
    struct timespec remaining = {
        .tv_sec = (time_t)(configured_sleep_us / 1000000UL),
        .tv_nsec = (long)(configured_sleep_us % 1000000UL) * 1000L,
    };

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

__attribute__((visibility("default"))) int sched_yield(void) {
    int saved_errno = errno;

    if (!shim_initialized || configured_sleep_us == 0 ||
        !current_thread_is_raknet_recv()) {
        return call_real_sched_yield();
    }
    if (sleep_for_configured_backoff() != 0) {
        return call_real_sched_yield();
    }
    if (diagnostics_enabled) {
        atomic_fetch_add_explicit(&backoff_count, 1, memory_order_relaxed);
    }
    errno = saved_errno;
    return 0;
}

__attribute__((visibility("default"))) uint64_t
tgcompat_raknet_recv_backoff_count(void) {
    return atomic_load_explicit(&backoff_count, memory_order_relaxed);
}

__attribute__((visibility("default"))) unsigned long
tgcompat_raknet_recv_sleep_us(void) {
    return configured_sleep_us;
}
