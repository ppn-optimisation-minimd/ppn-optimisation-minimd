#!/bin/bash

# MiniMD benchmark
# Run from the project folder

cd "$(dirname "$0")/baseline/ref" || exit 1

export OMP_NUM_THREADS=1

if [ ! -f ./miniMD_openmpi ]; then
    echo "MiniMD is not compiled yet."
    echo "Please compile baseline/ref first."
    exit 1
fi

echo "Running MiniMD benchmark..."

for steps in 10 15
do
    for i in 1 2 3
    do
        echo "========== $steps steps - Run $i =========="

        mpirun -np 1 ./miniMD_openmpi \
            -i in.lj.miniMD \
            -s 100 \
            -n "$steps" \
            -t 1 \
            | grep -A 2 "Performance Summary"
    done
done

echo "Benchmark finished."
