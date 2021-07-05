#!/bin/bash
#SBATCH --account=mp215
#SBATCH --time=15:00
#SBATCH --job-name=kcount
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j
#SBATCH --partition=regular
#SBATCH --constraint=haswell

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/examples/kcount
DATASET=$SCRATCH/kcount/gut-5x.fastq

echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_agg"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_agg -r ${DATASET} -k 51
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_aggrd"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_aggrd -r ${DATASET} -k 51
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_ff"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_ff -r ${DATASET} -k 51
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_ffrd"
srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_ffrd -r ${DATASET} -k 51
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=32 kcount_upcxx"
srun --ntasks-per-node=32 ${PATH_TO_EXE}/kcount_upcxx -r ${DATASET} -k 51

echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_ffrd aggBuffer=simple"
ARL_AGG_BUFFER_TYPE=simple srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_ffrd -r ${DATASET} -k 51
echo "srun -N=${SLURM_NNODES} --ntasks-per-node=2 kcount_ffrd aggBuffer=local"
ARL_AGG_BUFFER_TYPE=local srun --ntasks-per-node=2 ${PATH_TO_EXE}/kcount_ffrd -r ${DATASET} -k 51