/*
 * cgroup.c - Part 3: Cgroup resource limits
 *
 * Implements cgroup v2 operations for CPU and memory limits.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include "minictl.h"

/*
 * cgroup_path - Generate cgroup path for container
 */
static void cgroup_path(pid_t pid, char *buf, size_t size) {
    snprintf(buf, size, "%s/minictl-%d", CGROUP_ROOT, pid);
}

int cgroup_create(pid_t pid) {

    char path[PATH_MAX];
    cgroup_path(pid, path, sizeof(path));


    if (mkdir(path, 0755) < 0 && errno != EEXIST) {
        perror("mkdir cgroup");
        return -1;
    }

    return 0;
}

int cgroup_set_memory(pid_t pid, uint64_t limit) {
    /*
     * Target: Set memory limit for container
     *
     * Write limit (in bytes) to:
     *   /sys/fs/cgroup/minictl-<pid>/memory.max
     *
     * Example: To limit to 64MB, write "67108864"
     */

    char path[PATH_MAX];
    char content[32];
    
    cgroup_path(pid, path, sizeof(path));
    strncat(path, "/memory.max", sizeof(path) - strlen(path) - 1);
    
    snprintf(content, sizeof(content), "%lu", limit);
    

    /* TODO: Uncomment the following code */
    /*
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open memory.max");
        return -1;
    }
    if (write(fd, content, strlen(content)) < 0) {
        perror("write memory.max");
        close(fd);
        return -1;
    }
    close(fd);
    */

    return 0;
}

int cgroup_set_cpu(pid_t pid, int percent) {
    /*
     * Target: Set CPU limit for container
     *
     * Write "quota period" to:
     *   /sys/fs/cgroup/minictl-<pid>/cpu.max
     *
     * CPU limit is expressed as quota/period:
     * - period = 100000 (100ms, standard period)
     * - quota = percent * 1000 (percent of one CPU)
     *
     * Example: To limit to 50% of one CPU:
     *   quota = 50 * 1000 = 50000
     *   Write "50000 100000" to cpu.max
     *
     * Example: To limit to 10% of one CPU:
     *   Write "10000 100000"
     */

    char path[PATH_MAX];
    char content[32];
    
    cgroup_path(pid, path, sizeof(path));
    strncat(path, "/cpu.max", sizeof(path) - strlen(path) - 1);
    
    int quota = percent * 1000;  /* microseconds per period */
    int period = 100000;         /* 100ms period */
    
    snprintf(content, sizeof(content), "%d %d", quota, period);
    

    /* TODO: add open/write/close here, same pattern as cgroup_set_memory */

    return 0;
}

int cgroup_add_process(pid_t pid) {
    /*
     * Target: Add process to the container's cgroup
     *
     * Write PID to:
     *   /sys/fs/cgroup/minictl-<pid>/cgroup.procs
     *
     * This makes the process subject to the cgroup's limits.
     * All descendants will also be in this cgroup.
     */

    char path[PATH_MAX];
    char content[32];
    
    cgroup_path(pid, path, sizeof(path));
    strncat(path, "/cgroup.procs", sizeof(path) - strlen(path) - 1);
    
    snprintf(content, sizeof(content), "%d", pid);
    

    /* TODO: add open/write/close here, same pattern as cgroup_set_memory */

    return 0;
}

int cgroup_cleanup(pid_t pid) {

    char path[PATH_MAX];
    cgroup_path(pid, path, sizeof(path));
    

    if (rmdir(path) < 0 && errno != ENOENT) {
        perror("rmdir cgroup");
        return -1;
    }

    return 0;
}
