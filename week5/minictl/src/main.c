/*
 * main.c - minictl entry point
 *
 * Usage:
 *   minictl chroot <rootfs> <cmd> [args...]
 *   minictl run [options] <rootfs> <cmd> [args...]
 *   minictl run-image <image-name>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minictl.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s chroot <rootfs> <cmd> [args...]      Part 1: Simple sandbox\n", prog);
    fprintf(stderr, "  %s run [options] <rootfs> <cmd> [args...] Part 2-3: Container\n", prog);
    fprintf(stderr, "  %s run-image <image-name>                Part 4: Run from image\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options for 'run':\n");
    fprintf(stderr, "  --hostname=NAME       Set container hostname\n");
    fprintf(stderr, "  --mem-limit=SIZE      Memory limit (e.g., 64M, 1G)\n");
    fprintf(stderr, "  --cpu-limit=PCT       CPU limit as percentage (e.g., 50)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s chroot /path/to/rootfs /bin/sh\n", prog);
    fprintf(stderr, "  %s run --hostname=mycontainer /path/to/rootfs /bin/sh\n", prog);
    fprintf(stderr, "  %s run --mem-limit=64M --cpu-limit=10 /rootfs /bin/sh\n", prog);
}

static int parse_run_options(int argc, char **argv, struct run_opts *opts) {
    static struct option long_options[] = {
        {"hostname",  required_argument, 0, 'h'},
        {"mem-limit", required_argument, 0, 'm'},
        {"cpu-limit", required_argument, 0, 'c'},
        {"help",      no_argument,       0, '?'},
        {0, 0, 0, 0}
    };

    memset(opts, 0, sizeof(*opts));

    int opt;
    int option_index = 0;

    /* Reset getopt */
    optind = 1;

    while ((opt = getopt_long(argc, argv, "+", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            opts->hostname = optarg;
            break;
        case 'm':
            opts->mem_limit = parse_size(optarg);
            if (opts->mem_limit == 0) {
                fprintf(stderr, "Invalid memory limit: %s\n", optarg);
                return -1;
            }
            break;
        case 'c':
            opts->cpu_limit = atoi(optarg);
            if (opts->cpu_limit <= 0 || opts->cpu_limit > 100) {
                fprintf(stderr, "Invalid CPU limit: %s (must be 1-100)\n", optarg);
                return -1;
            }
            break;
        case '?':
        default:
            return -1;
        }
    }

    /* Remaining arguments: rootfs cmd [args...] */
    if (optind >= argc) {
        fprintf(stderr, "Missing rootfs and command\n");
        return -1;
    }

    opts->rootfs = argv[optind++];

    if (optind >= argc) {
        fprintf(stderr, "Missing command\n");
        return -1;
    }

    opts->cmd = &argv[optind];
    opts->cmd_argc = argc - optind;

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "chroot") == 0) {
        /* Part 1: chroot mode */
        return cmd_chroot(argc - 2, &argv[2]);
    }
    else if (strcmp(command, "run") == 0) {
        /* Part 2-3: run mode with namespaces and cgroups */
        struct run_opts opts;
        if (parse_run_options(argc - 1, &argv[1], &opts) < 0) {
            print_usage(argv[0]);
            return 1;
        }
        return cmd_run(&opts);
    }
    else if (strcmp(command, "run-image") == 0) {
        /* Part 4: run from image */
        if (argc < 3) {
            fprintf(stderr, "Usage: %s run-image <image-name>\n", argv[0]);
            return 1;
        }
        return cmd_run_image(argv[2], NULL);
    }
    else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
