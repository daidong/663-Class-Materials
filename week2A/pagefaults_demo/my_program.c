// my_program.c
// A small workload that intentionally touches memory to trigger page faults.
//
// Build:  gcc -O2 -Wall -Wextra -std=c11 -o my_program my_program.c
// Usage examples (see README.md for more):
//   ./my_program --anon 512 --pattern seq --repeat 1
//   /usr/bin/time -v ./my_program --anon 512 --pattern seq --repeat 1 2>&1 | grep -i fault
//   sudo perf stat -e page-faults,major-faults ./my_program --anon 512 --pattern seq --repeat 1
//
// Notes:
// - Anonymous memory first-touch faults are typically *minor* (no disk I/O).
// - Major faults usually require file-backed mappings that miss the page cache,
//   or swapped-out pages under memory pressure.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static size_t parse_size_mb(const char *s) {
    char *end = NULL;
    long mb = strtol(s, &end, 10);
    if (!s[0] || (end && *end != '\0') || mb <= 0) {
        fprintf(stderr, "Invalid size MB: %s\n", s);
        exit(2);
    }
    return (size_t)mb * 1024ULL * 1024ULL;
}

static long parse_long(const char *s, const char *name, long minv) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!s[0] || (end && *end != '\0') || v < minv) {
        fprintf(stderr, "Invalid %s: %s\n", name, s);
        exit(2);
    }
    return v;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --anon <MB> [--pattern seq|rand] [--repeat N] [--madvise-dontneed]\n"
            "  %s --file <path> --file-mb <MB> [--pattern seq|rand] [--repeat N] [--fadvise-dontneed]\n"
            "\n"
            "Options:\n"
            "  --anon <MB>            Allocate anonymous memory and touch it.\n"
            "  --file <path>          Use a file-backed mmap (creates/truncates file).\n"
            "  --file-mb <MB>         File size / mapping size.\n"
            "  --pattern seq|rand     Touch one byte per page sequentially or randomly (default: seq).\n"
            "  --repeat N             Repeat the touching loop N times (default: 1).\n"
            "  --madvise-dontneed     After each iteration, madvise(MADV_DONTNEED) the mapping (anon mode).\n"
            "  --fadvise-dontneed     After each iteration, posix_fadvise(DONTNEED) the file (file mode; best-effort).\n"
            "\n"
            "Tip: First iteration usually causes most faults; later iterations should be warm (few faults),\n"
            "unless you use DONTNEED options to re-introduce faults.\n",
            argv0, argv0);
    exit(2);
}

static void shuffle(size_t *a, size_t n) {
    // Fisher-Yates shuffle
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rand() % (int)(i + 1));
        size_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
}

static uint64_t touch_pages(char *buf, size_t len, bool random_order) {
    const size_t page = (size_t)getpagesize();
    size_t pages = (len + page - 1) / page;

    size_t *idx = (size_t *)malloc(pages * sizeof(size_t));
    if (!idx) die("malloc idx");
    for (size_t i = 0; i < pages; i++) idx[i] = i;
    if (random_order) shuffle(idx, pages);

    volatile uint64_t sum = 0;
    for (size_t i = 0; i < pages; i++) {
        size_t off = idx[i] * page;
        if (off >= len) break;
        // Read + write to ensure the page is faulted in and dirtied (anon mode).
        sum += (unsigned char)buf[off];
        buf[off] = (char)((buf[off] + 1) & 0x7f);
    }

    free(idx);
    return (uint64_t)sum;
}

static int ensure_file_size(int fd, size_t size) {
    // ftruncate is enough for sparse files; actual disk I/O depends on filesystem.
    if (ftruncate(fd, (off_t)size) != 0) return -1;

    // Touch some bytes so the file has content; keep it light.
    // (We write one byte per page. This is not needed for mmap itself, but makes the demo less confusing.)
    const size_t page = (size_t)getpagesize();
    char one = 'x';
    for (size_t off = 0; off < size; off += page) {
        if (pwrite(fd, &one, 1, (off_t)off) != 1) {
            return -1;
        }
    }
    if (fsync(fd) != 0) return -1;
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = NULL; // "anon" or "file"
    const char *file_path = NULL;
    size_t size = 0;
    size_t file_size = 0;

    bool pattern_rand = false;
    long repeat = 1;
    bool madvise_dontneed = false;
    bool fadvise_dontneed = false;

    if (argc < 2) usage(argv[0]);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--anon") == 0) {
            if (i + 1 >= argc) usage(argv[0]);
            mode = "anon";
            size = parse_size_mb(argv[++i]);
        } else if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) usage(argv[0]);
            mode = "file";
            file_path = argv[++i];
        } else if (strcmp(argv[i], "--file-mb") == 0) {
            if (i + 1 >= argc) usage(argv[0]);
            file_size = parse_size_mb(argv[++i]);
        } else if (strcmp(argv[i], "--pattern") == 0) {
            if (i + 1 >= argc) usage(argv[0]);
            const char *p = argv[++i];
            if (strcmp(p, "seq") == 0) pattern_rand = false;
            else if (strcmp(p, "rand") == 0) pattern_rand = true;
            else usage(argv[0]);
        } else if (strcmp(argv[i], "--repeat") == 0) {
            if (i + 1 >= argc) usage(argv[0]);
            repeat = parse_long(argv[++i], "repeat", 1);
        } else if (strcmp(argv[i], "--madvise-dontneed") == 0) {
            madvise_dontneed = true;
        } else if (strcmp(argv[i], "--fadvise-dontneed") == 0) {
            fadvise_dontneed = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage(argv[0]);
        }
    }

    // Seed RNG for random page order.
    srand((unsigned)time(NULL));

    if (!mode) usage(argv[0]);

    int fd = -1;
    char *buf = NULL;

    if (strcmp(mode, "anon") == 0) {
        if (size == 0) usage(argv[0]);
        buf = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buf == MAP_FAILED) die("mmap anon");
    } else if (strcmp(mode, "file") == 0) {
        if (!file_path || file_size == 0) usage(argv[0]);
        size = file_size;

        fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) die("open file");
        if (ensure_file_size(fd, size) != 0) die("ensure_file_size");

        buf = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buf == MAP_FAILED) die("mmap file");
    } else {
        usage(argv[0]);
    }

    uint64_t total = 0;
    for (long r = 0; r < repeat; r++) {
        total += touch_pages(buf, size, pattern_rand);

        if (madvise_dontneed && strcmp(mode, "anon") == 0) {
            if (madvise(buf, size, MADV_DONTNEED) != 0) {
                die("madvise(MADV_DONTNEED)");
            }
        }

        if (fadvise_dontneed && strcmp(mode, "file") == 0) {
            // Best-effort: tell kernel we don't need these pages in page cache.
            // This does not guarantee major faults next run; it depends on cache pressure.
            int rc = posix_fadvise(fd, 0, (off_t)size, POSIX_FADV_DONTNEED);
            if (rc != 0) {
                errno = rc;
                die("posix_fadvise(DONTNEED)");
            }
        }
    }

    // Prevent optimization removal.
    printf("Done. checksum=%" PRIu64 "\n", total);

    if (munmap(buf, size) != 0) die("munmap");
    if (fd >= 0) close(fd);
    return 0;
}
