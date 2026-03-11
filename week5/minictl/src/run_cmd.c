/*
 * run_cmd.c - Part 2: Container with namespaces
 *
 * Implements: minictl run [options] <rootfs> <cmd> [args...]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <syscall.h>
#include "minictl.h"

static int pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}


/* Arguments passed to child function */
struct child_args {
    struct run_opts *opts;
    int pipe_fd[2];  /* Pipe for parent-child synchronization */
};

/*
 * setup_user_namespace - Parent sets up user namespace mappings
 *
 * This must be done from the parent after clone() but before
 * the child continues execution.
 */
static int setup_user_namespace(pid_t child_pid) {

    char path[PATH_MAX];
    char content[64];
    int fd;

    /* Step 1: Write "deny" to setgroups */
    snprintf(path, sizeof(path), "/proc/%d/setgroups", child_pid);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open setgroups");
        return -1;
    }
    if (write(fd, "deny", 4) < 0) {
        perror("write setgroups");
        close(fd);
        return -1;
    }
    close(fd);

    snprintf(path, sizeof(path), "/proc/%d/uid_map", child_pid);
    snprintf(content, sizeof(content), "0 %d 1", getuid());
    /* TODO: open, write, close */
    printf("TODO: Write '%s' to %s\n", content, path);

    snprintf(path, sizeof(path), "/proc/%d/gid_map", child_pid);
    snprintf(content, sizeof(content), "0 %d 1", getgid());
    /* TODO: open, write, close */
    printf("TODO: Write '%s' to %s\n", content, path);

    return 0;  /* Change to return actual status */
}

/*
 * setup_mounts - Set up mount namespace inside container
 *
 * Called from child after namespaces are created.
 */
static int setup_mounts(const char *rootfs) {
    /*
     * TODO: Implement mount namespace setup
     *
     * Targets:
     * 1. Make current mounts private (don't propagate to host):
     *    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)
     *
     * 2. Bind-mount rootfs to itself (required for pivot_root):
     *    mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL)
     *
     * 3. Create a directory for old root:
     *    mkdir(<rootfs>/old_root, 0755)
     *
     * 4. Pivot root:
     *    pivot_root(rootfs, <rootfs>/old_root)
     *
     * 5. Change to new root:
     *    chdir("/")
     *
     * 6. Unmount and remove old root:
     *    umount2("/old_root", MNT_DETACH)
     *    rmdir("/old_root")
     *
     * 7. Mount /proc:
     *    mount("proc", "/proc", "proc", 0, NULL)
     *
     * 8. Fallback: If pivot_root fails, fall back to chroot
     * 
     * 9. mount("proc", "/proc", "proc", 0, NULL); */

    return 0;
}

/*
 * child_func - Function executed by the cloned child
 *
 * This runs in the new namespaces.
 */
static int child_func(void *arg) {
    struct child_args *args = (struct child_args *)arg;
    struct run_opts *opts = args->opts;

    /* Wait for parent to set up user namespace mappings */
    close(args->pipe_fd[1]);  /* Close write end */
    char ch;
    if (read(args->pipe_fd[0], &ch, 1) != 1) {
        fprintf(stderr, "Failed to sync with parent\n");
        return 1;
    }
    close(args->pipe_fd[0]);

    /* Step 1: Set hostname */
    if (opts->hostname) {
        if (sethostname(opts->hostname, strlen(opts->hostname)) < 0) {
            perror("sethostname");
            /* Continue anyway - not fatal */
        }
    }

    /* Step 2: Set up mounts */
    if (setup_mounts(opts->rootfs) < 0) {
        return 1;
    }

    /* Step 3: Execute command */
    execvp(opts->cmd[0], opts->cmd);
    perror("execvp");
    return 1;
}

int cmd_run(struct run_opts *opts) {

    /* Step 1: Create synchronization pipe */
    struct child_args args = { .opts = opts };
    if (pipe(args.pipe_fd) < 0) {
        perror("pipe");
        return 1;
    }

    /* Step 2: Allocate stack for clone */
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        return 1;
    }
    char *stack_top = stack + STACK_SIZE;

    /* Step 3: Clone with namespace flags */
    int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;
    if (getuid() != 0) {
        flags |= CLONE_NEWUSER;  /* Only need user namespace when not root */
    }
    pid_t child = clone(child_func, stack_top, flags, &args);
    if (child < 0) {
        perror("clone");
        free(stack);
        return 1;
    }

    /* Step 4a: Set up user namespace mappings */
    close(args.pipe_fd[0]);  /* Close read end in parent */
    
    if (getuid() != 0) {
        if (setup_user_namespace(child) < 0) {
            fprintf(stderr, "Failed to set up user namespace\n");
        }
    }
    

    /* Step 4b-c: [Part 3] Cgroup setup */
    if (opts->mem_limit || opts->cpu_limit) {
        /*
        cgroup_create(child);
        if (opts->mem_limit) cgroup_set_memory(child, opts->mem_limit);
        if (opts->cpu_limit) cgroup_set_cpu(child, opts->cpu_limit);
        cgroup_add_process(child);
        */
    }

    /* Step 4d: Signal child to continue */
    if (write(args.pipe_fd[1], "x", 1) != 1) {
        perror("write to pipe");
    }
    close(args.pipe_fd[1]);

    /* Step 4e: Wait for child */
    int status;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        free(stack);
        return 1;
    }

    /* Step 4f: [Part 3] Clean up cgroup */
    if (opts->mem_limit || opts->cpu_limit) {
        /* cgroup_cleanup(child); */
    }

    free(stack);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return 1;
}
