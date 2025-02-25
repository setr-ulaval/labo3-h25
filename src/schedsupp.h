#include <linux/kernel.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <linux/types.h>
#include <sched.h>
#include <linux/sched.h>
#include <sys/types.h>
#include "utils.h"


struct sched_attr {
    __u32 size;

    __u32 sched_policy;
    __u64 sched_flags;

    /* SCHED_NORMAL, SCHED_BATCH */
    __s32 sched_nice;

    /* SCHED_FIFO, SCHED_RR */
    __u32 sched_priority;

    /* SCHED_DEADLINE */
    __u64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
};

int sched_setattr(pid_t pid,
              const struct sched_attr *attr,
              unsigned int flags)
{
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid,
              struct sched_attr *attr,
              unsigned int size,
              unsigned int flags)
{
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

void setOrdonnanceur(int modeOrdonnanceur, int runtime, int deadline, int period)
{
    int policy;
    struct sched_param param;
    struct sched_attr attr = {0};

    // FDM: Assuming runtime, deadline and period in ms

    runtime *= 1000000;
    deadline *= 1000000;
    period *= 1000000;
    
    if (sched_getparam(0, &param) != 0) {
        perror("sched_getparam failed");
        exit(EXIT_FAILURE);
    }
    switch (modeOrdonnanceur)
    {
    case ORDONNANCEMENT_NORT:
        policy = SCHED_NORMAL;
        break;
    case ORDONNANCEMENT_RR:
        policy = SCHED_RR;
        param.sched_priority = 1;
        break;
    case ORDONNANCEMENT_FIFO:
        policy = SCHED_FIFO;
        param.sched_priority = 1;
        break;
    case ORDONNANCEMENT_DEADLINE:
        policy = SCHED_DEADLINE;
        break;
    default:
        printf("Unknown scheduling mode!\n");
        exit(EXIT_FAILURE);
    }   

    if (policy == SCHED_DEADLINE) {
        // SCHED_DEADLINE requires `sched_setattr`
        attr.size = sizeof(struct sched_attr);
        attr.sched_policy = SCHED_DEADLINE;
        attr.sched_runtime = runtime;
        attr.sched_deadline = deadline;
        attr.sched_period = period;

        if (sched_setattr(0, &attr, 0) != 0) {
            perror("sched_setattr failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Use sched_setscheduler for other policies
        if (sched_setscheduler(0, policy, &param) != 0) {
            perror("sched_setscheduler failed");
            exit(EXIT_FAILURE);
        }
    }

    int current_policy = sched_getscheduler(0);
    if (current_policy == -1) {
        perror("sched_getscheduler failed");
        exit(EXIT_FAILURE);
    }

    if (current_policy == SCHED_DEADLINE) {
        struct sched_attr get_attr = {0};
        get_attr.size = sizeof(struct sched_attr);
        if (sched_getattr(0, &get_attr, sizeof(struct sched_attr), 0) != 0) {
            perror("sched_getattr failed");
            exit(EXIT_FAILURE);
        }
        printf(" Scheduler set to SCHED_DEADLINE\n");
        printf("   Runtime: %llu ns\n", get_attr.sched_runtime);
        printf("   Deadline: %llu ns\n", get_attr.sched_deadline);
        printf("   Period: %llu ns\n", get_attr.sched_period);
    } else {
        struct sched_param get_param;
        if (sched_getparam(0, &get_param) != 0) {
            perror("sched_getparam failed");
            exit(EXIT_FAILURE);
        }
        printf(" Scheduler set to: %s (Priority: %d)\n",
               (current_policy == SCHED_NORMAL) ? "SCHED_NORMAL" :
               (current_policy == SCHED_RR) ? "SCHED_RR" :
               (current_policy == SCHED_FIFO) ? "SCHED_FIFO" : "UNKNOWN",
               get_param.sched_priority);
    }
}