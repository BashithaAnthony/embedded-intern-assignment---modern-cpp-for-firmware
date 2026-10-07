#!/usr/bin/env bash
# Task 4: build the virtual and CRTP benchmarks with -Os, then print a size/speed table.
# Usage: ./scripts/bench_task4.sh [runs]      (default: best of 5 runs)
set -euo pipefail
cd "$(dirname "$0")/.."
RUNS="${1:-5}"
DIR=build-bench

cmake -S . -B "$DIR" -DCMAKE_BUILD_TYPE=MinSizeRel -DSANITIZE= >/dev/null
cmake --build "$DIR" --target bench_task4_virtual bench_task4_crtp -j >/dev/null

best_us () {   # run a benchmark $RUNS times, print the fastest time in microseconds
  local bin=$1 best=""
  for _ in $(seq "$RUNS"); do
    local us
    us=$("$bin" | sed -n 's/.*time_us=\([0-9]*\).*/\1/p')
    if [ -z "$best" ] || [ "$us" -lt "$best" ]; then best=$us; fi
  done
  echo "$best"
}

checksum () { "$1" | sed -n 's/.*checksum=\(-\?[0-9]*\).*/\1/p'; }

VBIN="$DIR/task4_sensors/bench_task4_virtual"
CBIN="$DIR/task4_sensors/bench_task4_crtp"

if [ "$(checksum "$VBIN")" != "$(checksum "$CBIN")" ]; then
  echo "ERROR: checksums differ, the two versions are not doing the same work" >&2
  exit 1
fi

echo
echo "g++ $(g++ -dumpfullversion), flags: -Os -ffunction-sections -fdata-sections -Wl,--gc-sections, best of $RUNS runs"
echo
echo "| Version | text (B) | data (B) | bss (B) | 10M read() calls | ns per call |"
echo "|---------|---------:|---------:|--------:|-----------------:|------------:|"
for v in virtual crtp; do
  bin="$DIR/task4_sensors/bench_task4_$v"
  read -r text data bss < <(size "$bin" | awk 'NR==2 {print $1, $2, $3}')
  us=$(best_us "$bin")
  awk -v v="$v" -v t="$text" -v d="$data" -v b="$bss" -v us="$us" \
    'BEGIN { printf "| %s | %d | %d | %d | %.1f ms | %.2f |\n", v, t, d, b, us/1000.0, us*1000.0/10000000.0 }'
done
echo
