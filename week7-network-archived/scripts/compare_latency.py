#!/usr/bin/env python3
"""
compare_latency.py - Compare latency results from wrk or hey

Usage:
    python3 compare_latency.py baseline.txt sidecar.txt
    python3 compare_latency.py --format hey baseline.txt sidecar.txt
"""

import sys
import re
import argparse

def parse_wrk_output(content):
    """Parse wrk output and extract metrics."""
    metrics = {}
    
    # Parse latency distribution
    latency_match = re.search(r'Latency Distribution.*?50%\s+([\d.]+)(\w+).*?75%\s+([\d.]+)(\w+).*?90%\s+([\d.]+)(\w+).*?99%\s+([\d.]+)(\w+)', 
                              content, re.DOTALL)
    if latency_match:
        def to_ms(val, unit):
            val = float(val)
            if unit == 'us':
                return val / 1000
            elif unit == 's':
                return val * 1000
            return val  # already ms
        
        metrics['p50_ms'] = to_ms(latency_match.group(1), latency_match.group(2))
        metrics['p75_ms'] = to_ms(latency_match.group(3), latency_match.group(4))
        metrics['p90_ms'] = to_ms(latency_match.group(5), latency_match.group(6))
        metrics['p99_ms'] = to_ms(latency_match.group(7), latency_match.group(8))
    
    # Parse throughput
    rps_match = re.search(r'Requests/sec:\s+([\d.]+)', content)
    if rps_match:
        metrics['rps'] = float(rps_match.group(1))
    
    transfer_match = re.search(r'Transfer/sec:\s+([\d.]+)(\w+)', content)
    if transfer_match:
        metrics['transfer_sec'] = transfer_match.group(1) + transfer_match.group(2)
    
    return metrics

def parse_hey_output(content):
    """Parse hey output and extract metrics."""
    metrics = {}
    
    # Parse response time histogram percentiles
    p50_match = re.search(r'50%\s+in\s+([\d.]+)\s+secs', content)
    p90_match = re.search(r'90%\s+in\s+([\d.]+)\s+secs', content)
    p99_match = re.search(r'99%\s+in\s+([\d.]+)\s+secs', content)
    
    if p50_match:
        metrics['p50_ms'] = float(p50_match.group(1)) * 1000
    if p90_match:
        metrics['p90_ms'] = float(p90_match.group(1)) * 1000
    if p99_match:
        metrics['p99_ms'] = float(p99_match.group(1)) * 1000
    
    # Parse requests/sec
    rps_match = re.search(r'Requests/sec:\s+([\d.]+)', content)
    if rps_match:
        metrics['rps'] = float(rps_match.group(1))
    
    return metrics

def compare_metrics(baseline, sidecar):
    """Compare two sets of metrics."""
    print("\n" + "=" * 70)
    print("LATENCY COMPARISON")
    print("=" * 70)
    print(f"{'Metric':<20} {'Baseline':<15} {'Sidecar':<15} {'Δ (abs)':<12} {'Δ (%)':<10}")
    print("-" * 70)
    
    for key in ['p50_ms', 'p75_ms', 'p90_ms', 'p99_ms']:
        if key in baseline and key in sidecar:
            b_val = baseline[key]
            s_val = sidecar[key]
            diff = s_val - b_val
            pct = (diff / b_val * 100) if b_val > 0 else 0
            
            label = key.replace('_ms', '').upper()
            print(f"{label:<20} {b_val:<15.3f} {s_val:<15.3f} {diff:<12.3f} {pct:>+8.1f}%")
    
    print("-" * 70)
    
    if 'rps' in baseline and 'rps' in sidecar:
        b_rps = baseline['rps']
        s_rps = sidecar['rps']
        diff = s_rps - b_rps
        pct = (diff / b_rps * 100) if b_rps > 0 else 0
        print(f"{'RPS':<20} {b_rps:<15.1f} {s_rps:<15.1f} {diff:<12.1f} {pct:>+8.1f}%")
    
    print("=" * 70)
    
    # Summary
    print("\nSUMMARY:")
    if 'p99_ms' in baseline and 'p99_ms' in sidecar:
        overhead = sidecar['p99_ms'] - baseline['p99_ms']
        print(f"  - p99 latency overhead: {overhead:.3f}ms")
        if baseline['p99_ms'] > 0:
            print(f"  - p99 latency increase: {overhead/baseline['p99_ms']*100:.1f}%")
    
    if 'rps' in baseline and 'rps' in sidecar:
        throughput_loss = baseline['rps'] - sidecar['rps']
        print(f"  - Throughput reduction: {throughput_loss:.1f} req/sec")
        if baseline['rps'] > 0:
            print(f"  - Throughput loss: {throughput_loss/baseline['rps']*100:.1f}%")

def generate_csv(baseline, sidecar, output_file='comparison.csv'):
    """Generate CSV comparison file."""
    import csv
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['metric', 'baseline', 'sidecar', 'difference', 'pct_increase'])
        
        for key in ['p50_ms', 'p75_ms', 'p90_ms', 'p99_ms', 'rps']:
            if key in baseline and key in sidecar:
                b_val = baseline[key]
                s_val = sidecar[key]
                diff = s_val - b_val
                pct = (diff / b_val * 100) if b_val > 0 else 0
                writer.writerow([key, f'{b_val:.3f}', f'{s_val:.3f}', f'{diff:.3f}', f'{pct:.1f}'])
    
    print(f"\nCSV written to: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Compare latency test results')
    parser.add_argument('baseline', help='Baseline results file')
    parser.add_argument('sidecar', help='Sidecar results file')
    parser.add_argument('--format', choices=['wrk', 'hey'], default='wrk',
                        help='Output format of the load testing tool')
    parser.add_argument('--csv', help='Output CSV file', default='comparison.csv')
    
    args = parser.parse_args()
    
    # Read files
    with open(args.baseline, 'r') as f:
        baseline_content = f.read()
    with open(args.sidecar, 'r') as f:
        sidecar_content = f.read()
    
    # Parse
    if args.format == 'wrk':
        baseline = parse_wrk_output(baseline_content)
        sidecar = parse_wrk_output(sidecar_content)
    else:
        baseline = parse_hey_output(baseline_content)
        sidecar = parse_hey_output(sidecar_content)
    
    if not baseline:
        print(f"Error: Could not parse baseline file. Check format.")
        sys.exit(1)
    if not sidecar:
        print(f"Error: Could not parse sidecar file. Check format.")
        sys.exit(1)
    
    # Compare
    compare_metrics(baseline, sidecar)
    
    # Generate CSV
    generate_csv(baseline, sidecar, args.csv)

if __name__ == '__main__':
    main()
