#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <cgroup_path> <command...>" >&2
  exit 2
fi

CG="$1"
shift

if [[ ! -d "$CG" ]]; then
  echo "cgroup path does not exist: $CG" >&2
  exit 2
fi

# Move this shell into the cgroup, then exec the command.
# Run with sudo if needed.

echo $$ > "$CG/cgroup.procs"
exec "$@"
