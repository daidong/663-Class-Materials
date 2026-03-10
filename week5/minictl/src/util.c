/*
 * util.c - Utility functions for minictl
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "minictl.h"

uint64_t parse_size(const char *str) {
    /*
     * Parse size strings like:
     *   "1234"   -> 1234 bytes
     *   "64K"    -> 64 * 1024 bytes
     *   "64M"    -> 64 * 1024 * 1024 bytes
     *   "1G"     -> 1 * 1024 * 1024 * 1024 bytes
     *
     * Returns 0 on error.
     */

    if (!str || !*str) {
        return 0;
    }

    char *end;
    uint64_t value = strtoull(str, &end, 10);
    
    if (end == str) {
        return 0;  /* No digits found */
    }

    /* Skip whitespace */
    while (isspace(*end)) end++;

    /* Check suffix */
    switch (toupper(*end)) {
    case '\0':
        /* No suffix - bytes */
        break;
    case 'K':
        value *= 1024ULL;
        break;
    case 'M':
        value *= 1024ULL * 1024;
        break;
    case 'G':
        value *= 1024ULL * 1024 * 1024;
        break;
    default:
        return 0;  /* Unknown suffix */
    }

    return value;
}

int write_file(const char *path, const char *content) {
    /*
     * Write content to a file.
     * Opens file, writes content, closes file.
     * Returns 0 on success, -1 on error.
     */

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    ssize_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    
    if (written < 0) {
        perror(path);
        close(fd);
        return -1;
    }

    if (written != len) {
        fprintf(stderr, "%s: short write\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

ssize_t read_file(const char *path, char *buf, size_t size) {
    /*
     * Read entire file into buffer.
     * Returns number of bytes read, or -1 on error.
     */

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    ssize_t n = read(fd, buf, size - 1);
    if (n < 0) {
        perror(path);
        close(fd);
        return -1;
    }

    buf[n] = '\0';  /* Null-terminate */
    close(fd);
    return n;
}
