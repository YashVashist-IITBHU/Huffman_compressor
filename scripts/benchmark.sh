#!/usr/bin/env bash
# Builds the project, generates sample data, and benchmarks each file.
# Usage: ./scripts/benchmark.sh [size_mb] [runs]
set -euo pipefail

SIZE_MB="${1:-10}"
RUNS="${2:-3}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

make -s all
./bin/gen_samples samples "$SIZE_MB"
echo

CSV="$(mktemp)"
for f in samples/*; do
    ./bin/huffman bench "$f" "$RUNS" | tee /dev/stderr | grep '^CSV,' | cut -d, -f2- >> "$CSV"
    echo >&2
done 2>&1

echo "================================ Summary ================================"
printf "%-12s %12s %12s %8s %9s %12s %12s %6s\n" \
       "File" "Original" "Compressed" "Ratio" "Saved%" "Comp(ms)" "Decomp(ms)" "Check"
while IFS=, read -r name orig comp ratio saved cms dms ok; do
    printf "%-12s %12s %12s %8s %9s %12s %12s %6s\n" "$name" "$orig" "$comp" "$ratio" "$saved" "$cms" "$dms" "$ok"
done < "$CSV"
rm -f "$CSV"
