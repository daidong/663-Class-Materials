# lab2_latcli: per-iteration latency CLI

`latcli` is a small C program used in Lab 2.

It prints one line per iteration:

```
iter=123 latency_us=847
```

The goal is to produce many latency samples, then compute p50/p90/p99.

## Build

```bash
make -C week2B/lab2_latcli
```

## Run (baseline)

```bash
./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 --touch-per-iter 64 > baseline.log
python3 week2B/lab2_latcli/scripts/latency_to_csv.py baseline.log baseline.csv
python3 week2B/lab2_latcli/scripts/percentiles.py baseline.csv
```

## Run under a cgroup memory limit (example)

```bash
sudo mkdir -p /sys/fs/cgroup/lab2
printf "%d" $((512*1024*1024)) | sudo tee /sys/fs/cgroup/lab2/memory.max

sudo bash week2B/lab2_latcli/scripts/run_in_cgroup.sh /sys/fs/cgroup/lab2 \
  ./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 --touch-per-iter 64 > pressured.log
```

## Parameters

- `--iters`: number of iterations (samples)
- `--workset-mb`: size of allocated working set
- `--stride`: bytes between touches (default: 4096, roughly one byte per page)
- `--touch-per-iter`: number of touch operations per iteration

## Notes

- Under a tight `memory.max`, touching a large working set can trigger reclaim and (if swap is enabled) paging.
- If you see OOM-kill, reduce `--workset-mb` or increase `memory.max`.
