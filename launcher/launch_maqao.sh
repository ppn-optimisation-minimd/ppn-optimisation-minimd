#!/bin/bash

THREADS=1
FILE=in.lj.miniMD
REPO=baseline
VERSION=ref

MAQAO_PATH="$(pwd)/../maqao.2026.1.0/bin/maqao"
MINIMD_PATH="$(pwd)/../${REPO}/${VERSION}/miniMD_openmpi"

${MAQAO_PATH} oneview -R1 -- ./mpirun -np ${THREADS} ${MINIMD_PATH} -i ${FILE}
