#!/bin/bash
#SBATCH --account=mp215
#SBATCH --time=5:00
#SBATCH --job-name=heavy_handler
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j
#SBATCH --partition=regular
#SBATCH --constraint=haswell

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/examples/microbenchmark/heavy_handler

echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 arl_hh_agg"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_hh_agg
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 arl_hh_aggrd"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_hh_aggrd
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 arl_hh_ff"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_hh_ff
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 arl_hh_ffrd"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/arl_hh_ffrd
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 gex_hh"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/gex_hh
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=32 upcxx_hh_agg"
srun --ntasks-per-node=32 ${PATH_TO_EXE}/upcxx_hh_agg
