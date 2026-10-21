#!/bin/bash

THREADS=1
FILE=in.lj.miniMD
REPO=baseline
VERSION=ref
MOMENT=$(date '+%Y-%m-%d_%H-%M-%S')

MAQAO_PATH="../maqao.2026.1.0/bin/maqao"
MINIMD_PATH="../${REPO}/${VERSION}/miniMD_openmpi"

${MAQAO_PATH} oneview -R1 --xp=MAQAO_${MOMENT}_${REPO} --show-program-output -- ${MINIMD_PATH} -i ${FILE}
