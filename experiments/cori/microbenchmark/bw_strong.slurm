#!/bin/bash
#SBATCH --account=mp215
#SBATCH --time=5:00
#SBATCH --job-name=bw_strong
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j
#SBATCH --partition=regular
#SBATCH --constraint=haswell

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/examples/microbenchmark/bw_strong

echo "srun --ntasks-per-node=2 arl_agg_bw_strong"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_agg_bw_strong
echo "srun --ntasks-per-node=2 arl_aggrd_bw_strong"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_aggrd_bw_strong
echo "srun --ntasks-per-node=2 arl_ff_bw_strong"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_ff_bw_strong
echo "srun --ntasks-per-node=2 arl_ffrd_bw_strong"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_ffrd_bw_strong
echo "srun --ntasks-per-node=2 gex_bw_strong"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/gex_bw_strong
echo "srun --ntasks-per-node=32 upcxx_agg_bw_strong"
srun --ntasks-per-node=32 ${PATH_TO_EXE}/upcxx_agg_bw_strong
