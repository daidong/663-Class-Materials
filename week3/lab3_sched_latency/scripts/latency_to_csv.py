#!/usr/bin/env python3
import csv
import re
import sys

LINE_RE = re.compile(r"iter=(\d+)\s+latency_us=(\d+)")


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.log> <output.csv>", file=sys.stderr)
        return 2

    inp, outp = sys.argv[1], sys.argv[2]
    rows = 0

    with open(inp, "r", encoding="utf-8", errors="ignore") as fin, open(outp, "w", newline="", encoding="utf-8") as fout:
        w = csv.writer(fout)
        w.writerow(["iter", "latency_us"])
        for line in fin:
            m = LINE_RE.search(line)
            if not m:
                continue
            w.writerow([int(m.group(1)), int(m.group(2))])
            rows += 1

    if rows == 0:
        print("No samples parsed. Expected lines like: iter=123 latency_us=847", file=sys.stderr)
        return 1

    print(f"Wrote {rows} samples to {outp}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
