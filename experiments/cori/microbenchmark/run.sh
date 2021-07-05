#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

#TASKS=("bw_weak.slurm" "bw_strong.slurm" "latency.slurm" "attentiveness.slurm" "heavy_handler.slurm" "kcount.slurm")
TASKS=("kcount.slurm")
BUILD_ROOT=$(realpath "${BUILD_ROOT:-init/build}")
#NNODES=(16 32)
NNODES=(1 2 4 8 16 32)

# create the ./run directory
mkdir_s ./run
cd run

for i in $(eval echo {1..${1:-1}}); do
  for task in "${TASKS[@]}"; do
    if [ "$task.x" = "bw_weak.x" ]; then
      if [[ ! ${SLURM_NTASKS} =~ ^(32)$ ]]; then
        continue
      fi
    fi
    for nnodes in "${NNODES[@]}"; do
      sbatch -N ${nnodes} ../${task} ${BUILD_ROOT} || { echo "sbatch error!"; exit 1; }
    done
  done
done