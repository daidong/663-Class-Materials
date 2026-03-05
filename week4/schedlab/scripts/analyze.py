#!/usr/bin/env python3
"""
analyze.py - Analyze SchedLab output CSVs

Usage:
    python3 analyze.py latency.csv           # Analyze latency distribution
    python3 analyze.py --fairness fair.csv   # Analyze fairness data
    python3 analyze.py --compare idle.csv loaded.csv  # Compare two latency files
"""

import sys
import argparse
import numpy as np

def load_latency_csv(filename):
    """Load latency histogram CSV."""
    buckets = []
    counts = []
    with open(filename, 'r') as f:
        next(f)  # Skip header
        for line in f:
            parts = line.strip().split(',')
            if len(parts) == 2:
                buckets.append(float(parts[0]))
                counts.append(int(parts[1]))
    return np.array(buckets), np.array(counts)

def compute_percentiles(buckets, counts, percentiles=[50, 90, 99]):
    """Compute percentiles from histogram data."""
    total = np.sum(counts)
    cumulative = np.cumsum(counts)
    
    results = {}
    for p in percentiles:
        target = total * p / 100
        idx = np.searchsorted(cumulative, target)
        if idx < len(buckets):
            results[p] = buckets[idx]
        else:
            results[p] = buckets[-1]
    return results

def analyze_latency(filename):
    """Analyze a single latency CSV."""
    print(f"=== Analyzing: {filename} ===\n")
    
    buckets, counts = load_latency_csv(filename)
    total = np.sum(counts)
    
    # Expand histogram to samples for statistics
    samples = np.repeat(buckets, counts)
    
    print(f"Total samples: {total}")
    print(f"Mean latency: {np.mean(samples):.2f} us")
    print(f"Std dev: {np.std(samples):.2f} us")
    print(f"Min: {np.min(samples):.2f} us")
    print(f"Max: {np.max(samples):.2f} us")
    print()
    
    percentiles = compute_percentiles(buckets, counts, [50, 75, 90, 95, 99, 99.9])
    print("Percentiles:")
    for p, v in percentiles.items():
        print(f"  p{p}: {v:.2f} us")
    print()
    
    # Tail analysis
    p50 = percentiles[50]
    p99 = percentiles[99]
    print(f"Tail ratio (p99/p50): {p99/p50:.2f}x")

def compare_latencies(file1, file2):
    """Compare two latency distributions."""
    print(f"=== Comparing: {file1} vs {file2} ===\n")
    
    b1, c1 = load_latency_csv(file1)
    b2, c2 = load_latency_csv(file2)
    
    s1 = np.repeat(b1, c1)
    s2 = np.repeat(b2, c2)
    
    print(f"{'Metric':<15} {file1:<20} {file2:<20}")
    print(f"{'------':<15} {'------':<20} {'------':<20}")
    print(f"{'Samples':<15} {len(s1):<20} {len(s2):<20}")
    print(f"{'Mean (us)':<15} {np.mean(s1):<20.2f} {np.mean(s2):<20.2f}")
    print(f"{'Std (us)':<15} {np.std(s1):<20.2f} {np.std(s2):<20.2f}")
    
    p1 = compute_percentiles(b1, c1)
    p2 = compute_percentiles(b2, c2)
    
    print(f"{'p50 (us)':<15} {p1[50]:<20.2f} {p2[50]:<20.2f}")
    print(f"{'p90 (us)':<15} {p1[90]:<20.2f} {p2[90]:<20.2f}")
    print(f"{'p99 (us)':<15} {p1[99]:<20.2f} {p2[99]:<20.2f}")
    print()
    
    print(f"p99 ratio (file2/file1): {p2[99]/p1[99]:.2f}x")

def analyze_fairness(filename):
    """Analyze fairness CSV."""
    print(f"=== Analyzing Fairness: {filename} ===\n")
    
    pids = []
    run_times = []
    wait_times = []
    switches = []
    
    with open(filename, 'r') as f:
        next(f)  # Skip header
        for line in f:
            parts = line.strip().split(',')
            if len(parts) >= 4:
                pids.append(int(parts[0]))
                run_times.append(float(parts[1]))
                wait_times.append(float(parts[2]))
                switches.append(int(parts[3]))
    
    run_times = np.array(run_times)
    wait_times = np.array(wait_times)
    
    print(f"Tasks tracked: {len(pids)}")
    print()
    
    # Filter to tasks with significant run time
    significant = run_times > 1.0  # > 1ms
    if np.sum(significant) > 0:
        sig_run = run_times[significant]
        sig_wait = wait_times[significant]
        
        print("Tasks with >1ms run time:")
        print(f"  Count: {len(sig_run)}")
        print(f"  Mean run time: {np.mean(sig_run):.2f} ms")
        print(f"  Std run time: {np.std(sig_run):.2f} ms")
        print(f"  Mean wait time: {np.mean(sig_wait):.2f} ms")
        print()
        
        # Fairness metric: coefficient of variation
        if np.mean(sig_run) > 0:
            cv = np.std(sig_run) / np.mean(sig_run)
            print(f"Run time coefficient of variation: {cv:.2f}")
            if cv < 0.1:
                print("  -> Very fair (CV < 0.1)")
            elif cv < 0.3:
                print("  -> Reasonably fair (CV < 0.3)")
            else:
                print("  -> Unfair (CV >= 0.3)")

def main():
    parser = argparse.ArgumentParser(description='Analyze SchedLab output')
    parser.add_argument('files', nargs='+', help='CSV files to analyze')
    parser.add_argument('--fairness', action='store_true', help='Analyze fairness data')
    parser.add_argument('--compare', action='store_true', help='Compare two latency files')
    
    args = parser.parse_args()
    
    if args.compare and len(args.files) == 2:
        compare_latencies(args.files[0], args.files[1])
    elif args.fairness:
        for f in args.files:
            analyze_fairness(f)
    else:
        for f in args.files:
            analyze_latency(f)

if __name__ == '__main__':
    main()
