#!/usr/bin/env python3
import csv
import math
import sys


def percentile(sorted_vals, p):
    if not sorted_vals:
        return None
    if p <= 0:
        return sorted_vals[0]
    if p >= 100:
        return sorted_vals[-1]

    k = (len(sorted_vals) - 1) * (p / 100.0)
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return sorted_vals[int(k)]
    d0 = sorted_vals[int(f)] * (c - k)
    d1 = sorted_vals[int(c)] * (k - f)
    return d0 + d1


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <samples.csv>", file=sys.stderr)
        return 2

    path = sys.argv[1]
    vals = []

    with open(path, "r", encoding="utf-8") as f:
        r = csv.DictReader(f)
        if not r.fieldnames or "latency_us" not in r.fieldnames:
            print("CSV must contain a latency_us column", file=sys.stderr)
            return 2
        for row in r:
            try:
                vals.append(float(row["latency_us"]))
            except Exception:
                pass

    if not vals:
        print("No latency samples found.", file=sys.stderr)
        return 1

    vals.sort()
    p50 = percentile(vals, 50)
    p90 = percentile(vals, 90)
    p99 = percentile(vals, 99)

    print(f"count={len(vals)}")
    print(f"p50_us={p50:.2f}")
    print(f"p90_us={p90:.2f}")
    print(f"p99_us={p99:.2f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
