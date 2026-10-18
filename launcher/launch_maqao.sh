#!/bin/bash

THREADS=1
FILE=in.param_analyse.miniMD
REPO=baseline
VERSION=ref
MOMENT=$(date '+%Y-%m-%d_%H-%M-%S')

RESULTS_PATH="../../results/${REPO}"
FILE_PATH="../../launcher/${FILE}"
MAQAO_PATH="../../maqao.2026.1.0/bin/maqao"
MINIMD_PATH="../${REPO}/${VERSION}"

cd ${MINIMD_PATH}
${MAQAO_PATH} oneview -R1 -uarch=METEOR_LAKE --mpi-command="mpirun -np ${THREADS}" --xp=${RESULTS_PATH}/MAQAO_${MOMENT}_${REPO} --show-program-output -- miniMD_openmpi -i ${FILE_PATH} -s 100
