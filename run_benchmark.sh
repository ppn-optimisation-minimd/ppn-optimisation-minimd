
#!/bin/bash
set -euo pipefail

# MiniMD baseline benchmark
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MINIMD_DIR="$SCRIPT_DIR/baseline/ref"
RESULTS_DIR="$SCRIPT_DIR/results/baseline"

mkdir -p "$RESULTS_DIR"
cd "$MINIMD_DIR"

export OMP_NUM_THREADS=1

if [ ! -f ./miniMD_openmpi ]; then
    echo "Error: MiniMD is not compiled."
    exit 1
fi

echo "Starting MiniMD baseline benchmark"

for steps in 10 15
do
    for i in 1 2 3
    do
        LOG="$RESULTS_DIR/steps_${steps}_run_${i}.log"

        echo "========== $steps steps - Run $i =========="

        mpirun -np 1 ./miniMD_openmpi \
            -i in.lj.miniMD \
            -s 100 \
            -n "$steps" \
            -t 1 \
            > "$LOG" 2>&1

        grep -A 2 "Performance Summary" "$LOG" || true

        echo "Saved: $LOG"
    done
done

echo "Benchmark finished."
