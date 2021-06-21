#!/bin/bash
#SBATCH --account=mp215
#SBATCH --time=5:00
#SBATCH --job-name=attentiveness
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j
#SBATCH --partition=regular
#SBATCH --constraint=haswell

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/examples/microbenchmark/attentiveness

echo "srun --ntasks-per-node=2 arl_at"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_at
echo "srun --ntasks-per-node=32 upcxx_at"
srun --ntasks-per-node=32 ${PATH_TO_EXE}/upcxx_at
