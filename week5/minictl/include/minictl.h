/*
 * minictl.h - Mini Container Runtime Header
 *
 * This header defines the interface for the minictl container runtime.
 */

#ifndef MINICTL_H
#define MINICTL_H

#include <stdint.h>
#include <sys/types.h>

/* Stack size for clone() */
#define STACK_SIZE (1024 * 1024)  /* 1MB */

/* Default cgroup path */
#define CGROUP_ROOT "/sys/fs/cgroup"

/* ===========================================
 * Command Line Options
 * =========================================== */

struct run_opts {
    const char *rootfs;        /* Path to root filesystem */
    char **cmd;                /* Command to execute */
    int cmd_argc;              /* Number of command arguments */
    const char *hostname;      /* Container hostname (optional) */
    uint64_t mem_limit;        /* Memory limit in bytes (0 = no limit) */
    int cpu_limit;             /* CPU limit as percentage (0 = no limit) */
};

/* ===========================================
 * Part 1: chroot Command
 * =========================================== */

/*
 * cmd_chroot - Run a command in a chroot sandbox
 *
 * @argc: Number of arguments (excluding "chroot" itself)
 * @argv: Arguments: <rootfs> <cmd> [args...]
 *
 * Returns: Exit status of the command, or -1 on error
 */
int cmd_chroot(int argc, char **argv);

/* ===========================================
 * Part 2: run Command (Namespaces)
 * =========================================== */

/*
 * cmd_run - Run a command in an isolated container
 *
 * @opts: Run options including rootfs, command, hostname, limits
 *
 * Returns: Exit status of the command, or -1 on error
 */
int cmd_run(struct run_opts *opts);

/* ===========================================
 * Part 3: Cgroup Functions
 * =========================================== */

/*
 * cgroup_create - Create a cgroup for the container
 *
 * @pid: PID of the container process
 *
 * Returns: 0 on success, -1 on error
 */
int cgroup_create(pid_t pid);

/*
 * cgroup_set_memory - Set memory limit for container
 *
 * @pid: PID of the container process
 * @limit: Memory limit in bytes
 *
 * Returns: 0 on success, -1 on error
 */
int cgroup_set_memory(pid_t pid, uint64_t limit);

/*
 * cgroup_set_cpu - Set CPU limit for container
 *
 * @pid: PID of the container process
 * @percent: CPU percentage (1-100 for one CPU)
 *
 * Returns: 0 on success, -1 on error
 */
int cgroup_set_cpu(pid_t pid, int percent);

/*
 * cgroup_add_process - Add process to container cgroup
 *
 * @pid: PID of the container process
 *
 * Returns: 0 on success, -1 on error
 */
int cgroup_add_process(pid_t pid);

/*
 * cgroup_cleanup - Remove container cgroup
 *
 * @pid: PID used to create the cgroup
 *
 * Returns: 0 on success, -1 on error
 */
int cgroup_cleanup(pid_t pid);

/* ===========================================
 * Part 4: Image Functions (Optional)
 * =========================================== */

/*
 * cmd_run_image - Run a container from an image
 *
 * @image_name: Name of the image (looked up in images/ directory)
 * @override_opts: Options to override image config (can be NULL)
 *
 * Returns: Exit status of the command, or -1 on error
 */
int cmd_run_image(const char *image_name, struct run_opts *override_opts);

/* ===========================================
 * Utility Functions
 * =========================================== */

/*
 * parse_size - Parse size string like "64M" or "1G" to bytes
 *
 * @str: Size string
 *
 * Returns: Size in bytes, or 0 on error
 */
uint64_t parse_size(const char *str);

/*
 * write_file - Write string to a file
 *
 * @path: File path
 * @content: Content to write
 *
 * Returns: 0 on success, -1 on error
 */
int write_file(const char *path, const char *content);

/*
 * read_file - Read entire file into buffer
 *
 * @path: File path
 * @buf: Buffer to read into
 * @size: Buffer size
 *
 * Returns: Number of bytes read, or -1 on error
 */
ssize_t read_file(const char *path, char *buf, size_t size);

#endif /* MINICTL_H */
