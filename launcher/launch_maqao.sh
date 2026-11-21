#!/bin/bash

THREADS=1
FILE=in.param_analyse.miniMD
REPO=optim-llm
VERSION=ref
MOMENT=$(date '+%Y-%m-%d_%H-%M-%S')

RESULTS_PATH="../../results/${REPO}"
FILE_PATH="../../launcher/${FILE}"
MAQAO_PATH="../../maqao.2026.1.0/bin/maqao"
MINIMD_PATH="../${REPO}/${VERSION}"

cd ${MINIMD_PATH}
${MAQAO_PATH} oneview -R1 -uarch=TIGER_LAKE --mpi-command="mpirun -np ${THREADS}" --xp=${RESULTS_PATH}/MAQAO_${MOMENT}_${REPO} --show-program-output -- miniMD_openmpi -i ${FILE_PATH} -s 100



#### Baseline

####
# Timestep T U P Time
# 0 1.440000e+00 -6.773368e+00 -5.019670e+00  0.000
# 20 6.431060e-01 -5.584275e+00 8.210238e-01 31.401

# MPI_proc OMP_threads nsteps natoms t_total t_force t_neigh t_comm t_other performance perf/thread grep_string t_extra
# 1 1 20 4000000 32.887872 25.617656 5.853529 0.363389 1.053298 2432507.606919 2432507.606919 PERF_SUMMARY 0.213565
