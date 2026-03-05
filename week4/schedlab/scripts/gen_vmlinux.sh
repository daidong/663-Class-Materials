#!/bin/bash
# Generate vmlinux.h from running kernel
# This provides kernel type definitions for CO-RE (Compile Once - Run Everywhere)

set -e

OUTPUT="${1:-vmlinux.h}"

echo "Generating $OUTPUT from /sys/kernel/btf/vmlinux..."

if [ ! -f /sys/kernel/btf/vmlinux ]; then
    echo "ERROR: BTF not available at /sys/kernel/btf/vmlinux"
    echo ""
    echo "Your kernel may not have BTF enabled."
    echo "Check: cat /boot/config-\$(uname -r) | grep BTF"
    echo "It should show CONFIG_DEBUG_INFO_BTF=y"
    exit 1
fi

# Use bpftool to dump BTF as C header
sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > "$OUTPUT"

if [ -f "$OUTPUT" ]; then
    echo "Success! Generated $OUTPUT"
    echo "Size: $(wc -l < "$OUTPUT") lines"
else
    echo "ERROR: Failed to generate $OUTPUT"
    exit 1
fi
