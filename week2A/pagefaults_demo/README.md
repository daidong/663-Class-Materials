# Page Fault demo: `my_program`

This folder contains a tiny workload program used in the slides as `./my_program`.

## Build

```bash
cd week2A/pagefaults_demo
make
```

## 1) Minor faults (anonymous memory first-touch)

Anonymous memory (malloc/mmap) triggers page faults the first time you touch each page. On Linux these are typically **minor faults** (no disk I/O; kernel just allocates/zeros a page and updates page tables).

```bash
# Touch 512MB once, one byte per page
/usr/bin/time -v ./my_program --anon 512 --pattern seq --repeat 1 2>&1 | grep -i fault

# Or via perf
sudo perf stat -e page-faults,major-faults ./my_program --anon 512 --pattern seq --repeat 1
```

If you run it again, faults will usually be much lower because the pages are already resident.

To force the program to drop its anonymous pages between iterations (so you get faults again):

```bash
sudo perf stat -e page-faults,major-faults ./my_program --anon 512 --repeat 5 --madvise-dontneed
```

This still mostly creates **minor** faults.

## 2) Major faults (file-backed mapping, requires cache misses)

Major faults typically mean the kernel had to do **disk I/O** to satisfy the fault.
A common way to observe them is to fault in pages from a **file-backed `mmap`** when the file’s pages are not in the page cache.

This demo includes a file-backed mode:

```bash
# Create/truncate a 1024MB file and mmap it
sudo perf stat -e page-faults,major-faults ./my_program --file /tmp/pf_demo.bin --file-mb 1024 --repeat 1
```

Whether those faults show up as **major** depends on whether the data is already in the OS page cache.
To *increase* the chance of major faults, you can drop caches (requires root):

```bash
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
sudo perf stat -e page-faults,major-faults ./my_program --file /tmp/pf_demo.bin --file-mb 1024 --repeat 1
```

Or try the program’s best-effort cache-hint between repeats:

```bash
sudo perf stat -e page-faults,major-faults ./my_program --file /tmp/pf_demo.bin --file-mb 1024 --repeat 5 --fadvise-dontneed
```

Still: major faults are workload/system-dependent (RAM size, cache pressure, storage speed, etc.).

## What to replace `my_program` with in real experiments

Anything you want to measure:

```bash
/usr/bin/time -v ./qs 1000000 2>&1 | grep -i fault
sudo perf stat -e page-faults,major-faults ./qs 1000000
```
