/*
 * chroot_cmd.c - Part 1: Simple chroot sandbox
 *
 * Implements: minictl chroot <rootfs> <cmd> [args...]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "minictl.h"

int cmd_chroot(int argc, char **argv) {

    if (argc < 2) {
        fprintf(stderr, "Usage: minictl chroot <rootfs> <cmd> [args...]\n");
        return 1;
    }

    const char *rootfs = argv[0];
    char **cmd_args = &argv[1];


    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        /* Child process */
        
        // TODO: Uncomment and complete these steps:
        
        /* // Step 1: Change to rootfs directory
        if (chdir(rootfs) < 0) {
            perror("chdir to rootfs");
            exit(1);
        }

        // Step 2: Chroot into rootfs
        if (chroot(".") < 0) {
            perror("chroot");
            exit(1);
        }

        // Step 3: Change to / inside chroot
        // This is important! chroot doesn't change current directory
        if (chdir("/") < 0) {
            perror("chdir to /");
            exit(1);
        }

        // Step 4: Execute command
        execvp(cmd_args[0], cmd_args);
        perror("execvp");
        exit(1);
        */
        
    }

    /* Parent waits for child */
    int status;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return 1;
}
