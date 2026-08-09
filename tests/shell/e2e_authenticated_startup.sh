#!/usr/bin/env bash
# Authenticated-startup e2e: provision a real SQLite store, launch the real
# workstation binary against it offscreen, and require a clean reverse
# shutdown. Usage: e2e_authenticated_startup.sh <workstation-binary> <seed>.
set -eu

binary="$1"
seed="$2"
root="$(mktemp -d /tmp/squiflow-e2e.XXXXXX)"
trap 'rm -rf "$root"' EXIT

if ! "$seed" "$root/squiflow.db"; then
    echo "e2e: provisioning failed" >&2
    exit 1
fi

for directory in logs crash secrets cache; do
    mkdir -p "$root/$directory"
done

QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
    SQUIFLOW_DATA_ROOT="$root" SQUIFLOW_CACHE_ROOT="$root/cache" \
    "$binary" --smoke-test
code=$?
if [ "$code" -ne 0 ]; then
    echo "e2e: workstation exited $code against a provisioned store" >&2
    exit "$code"
fi

echo "e2e: authenticated startup and reverse shutdown passed"