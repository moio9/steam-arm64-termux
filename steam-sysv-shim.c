#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

#define SHIM_SEM_SETS 32
#define SHIM_SEMS_PER_SET 32
#define SHIM_SEM_ID_BASE 0x534d00

struct shim_sem_set {
    int used;
    key_t key;
    int count;
    unsigned short value[SHIM_SEMS_PER_SET];
};

static struct shim_sem_set sets[SHIM_SEM_SETS];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;

static struct shim_sem_set *from_id(int id)
{
    int slot = id - SHIM_SEM_ID_BASE;
    if (slot < 0 || slot >= SHIM_SEM_SETS || !sets[slot].used) {
        errno = EINVAL;
        return NULL;
    }
    return &sets[slot];
}

int semget(key_t key, int nsems, int flags)
{
    int free_slot = -1;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < SHIM_SEM_SETS; ++i) {
        if (sets[i].used && sets[i].key == key && key != IPC_PRIVATE) {
            if ((flags & IPC_CREAT) && (flags & IPC_EXCL)) {
                pthread_mutex_unlock(&lock);
                errno = EEXIST;
                return -1;
            }
            pthread_mutex_unlock(&lock);
            return SHIM_SEM_ID_BASE + i;
        }
        if (!sets[i].used && free_slot < 0) free_slot = i;
    }
    if (!(flags & IPC_CREAT) && key != IPC_PRIVATE) {
        pthread_mutex_unlock(&lock);
        errno = ENOENT;
        return -1;
    }
    if (free_slot < 0 || nsems <= 0 || nsems > SHIM_SEMS_PER_SET) {
        pthread_mutex_unlock(&lock);
        errno = free_slot < 0 ? ENOSPC : EINVAL;
        return -1;
    }
    memset(&sets[free_slot], 0, sizeof(sets[free_slot]));
    sets[free_slot].used = 1;
    sets[free_slot].key = key;
    sets[free_slot].count = nsems;
    pthread_mutex_unlock(&lock);
    return SHIM_SEM_ID_BASE + free_slot;
}

int semctl(int semid, int semnum, int cmd, ...)
{
    struct shim_sem_set *set;
    unsigned long arg = 0;
    va_list ap;
    if (cmd == SETVAL || cmd == SETALL) {
        va_start(ap, cmd);
        arg = va_arg(ap, unsigned long);
        va_end(ap);
    }
    pthread_mutex_lock(&lock);
    set = from_id(semid);
    if (!set) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    if (cmd == IPC_RMID) {
        memset(set, 0, sizeof(*set));
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    if (semnum < 0 || semnum >= set->count) {
        pthread_mutex_unlock(&lock);
        errno = EINVAL;
        return -1;
    }
    switch (cmd) {
    case GETVAL:
        arg = set->value[semnum];
        break;
    case SETVAL:
        if (arg > 32767) { errno = ERANGE; arg = (unsigned long)-1; break; }
        set->value[semnum] = (unsigned short)arg;
        pthread_cond_broadcast(&changed);
        arg = 0;
        break;
    case GETPID:
        arg = (unsigned long)getpid();
        break;
    case GETNCNT:
    case GETZCNT:
        arg = 0;
        break;
    default:
        errno = ENOTSUP;
        arg = (unsigned long)-1;
        break;
    }
    pthread_mutex_unlock(&lock);
    return (int)arg;
}

int semop(int semid, struct sembuf *ops, size_t count)
{
    struct shim_sem_set *set;
    pthread_mutex_lock(&lock);
    for (;;) {
        int blocked = 0;
        set = from_id(semid);
        if (!set) { pthread_mutex_unlock(&lock); return -1; }
        for (size_t i = 0; i < count; ++i) {
            unsigned short value;
            if (ops[i].sem_num >= set->count) {
                pthread_mutex_unlock(&lock);
                errno = EFBIG;
                return -1;
            }
            value = set->value[ops[i].sem_num];
            if ((ops[i].sem_op < 0 && value < (unsigned)-ops[i].sem_op) ||
                (ops[i].sem_op == 0 && value != 0)) {
                if (ops[i].sem_flg & IPC_NOWAIT) {
                    pthread_mutex_unlock(&lock);
                    errno = EAGAIN;
                    return -1;
                }
                blocked = 1;
                break;
            }
        }
        if (!blocked) break;
        pthread_cond_wait(&changed, &lock);
    }
    for (size_t i = 0; i < count; ++i)
        set->value[ops[i].sem_num] =
            (unsigned short)(set->value[ops[i].sem_num] + ops[i].sem_op);
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
    return 0;
}
