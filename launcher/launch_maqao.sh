#!/usr/bin/env bash

THREADS=1
FILE=in.param_analyse.miniMD
REPO=baseline
VERSION=ref
MOMENT=$(date '+%Y-%m-%d_%H-%M-%S')

RESULTS_PATH="results/${REPO}"
FILE_PATH="launcher/${FILE}"
MAQAO_PATH="maqao"
MINIMD_PATH="${REPO}/${VERSION}/miniMD_openmpi"

XP_PATH="${RESULTS_PATH}/MAQAO_${MOMENT}_${REPO}"

mkdir -p "${RESULTS_PATH}"
${MAQAO_PATH} oneview \
  -R1 \
  --mpi-command="mpirun -np ${THREADS}" \
  --xp=${XP_PATH} \
  --show-program-output \
  -- ${MINIMD_PATH} -i ${FILE_PATH} -s 100
