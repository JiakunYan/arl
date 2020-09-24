#!/bin/bash
#make clean && make
mpirun -n 8 ./benchmark_spmv $1 1 test
cat spmv_*.tmp | sort > spmv.log
rm spmv_*.tmp
mpirun -n 8 ./benchmark_spmv_naive $1 1 test
cat spmv_*.tmp | sort > spmv_naive.log
rm spmv_*.tmp
diff spmv_naive.log spmv.log