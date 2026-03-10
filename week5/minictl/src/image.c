/*
 * image.c - Part 4: Image support (OPTIONAL)
 *
 * Implements: minictl run-image <image-name>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "minictl.h"

/*
 * Image format:
 *
 * images/<image-name>/
 *   rootfs/         - Root filesystem
 *   config.txt      - Configuration file
 *
 * config.txt format (one key=value per line):
 *   entrypoint=/bin/sh
 *   args=-c echo hello
 *   hostname=mycontainer
 *   mem_limit=128M
 *   cpu_limit=50
 */

static int parse_config(const char *config_path, struct run_opts *opts) {
    /*
     * TODO: Parse config.txt and populate opts
     *
     * Steps:
     * 1. Open config_path
     * 2. Read line by line
     * 3. For each line, parse key=value
     * 4. Set corresponding field in opts
     *
     * Supported keys:
     *   entrypoint  -> opts->cmd[0]
     *   args        -> opts->cmd[1...]
     *   hostname    -> opts->hostname
     *   mem_limit   -> opts->mem_limit (use parse_size)
     *   cpu_limit   -> opts->cpu_limit
     */

    printf("TODO: parse_config not implemented\n");
    printf("  config_path: %s\n", config_path);
    
    return -1;
}

int cmd_run_image(const char *image_name, struct run_opts *override_opts) {
    /*
     * TODO: Implement Part 4
     *
     * Steps:
     * 1. Construct paths:
     *    rootfs_path = images/<image_name>/rootfs
     *    config_path = images/<image_name>/config.txt
     *
     * 2. Verify rootfs exists
     *
     * 3. Parse config.txt into run_opts
     *
     * 4. Apply any overrides from override_opts
     *
     * 5. Call cmd_run() with the populated opts
     */

    printf("run-image: Not yet implemented\n");
    printf("  image_name: %s\n", image_name);

    char rootfs_path[PATH_MAX];
    char config_path[PATH_MAX];

    snprintf(rootfs_path, sizeof(rootfs_path), "images/%s/rootfs", image_name);
    snprintf(config_path, sizeof(config_path), "images/%s/config.txt", image_name);

    printf("  rootfs_path: %s\n", rootfs_path);
    printf("  config_path: %s\n", config_path);

    /* TODO: Implement image loading and call cmd_run */

    return 1;
}
