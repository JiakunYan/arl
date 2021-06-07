#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

# get the Fabric Bench source path via environment variable or default value
ARL_SOURCE_PATH=$(realpath "${ARL_SOURCE_PATH:-../../../}")
GASNETEX_ROOT=$(realpath "../../external/GASNet-2021.3.0-large-medium")

if [[ -f "${ARL_SOURCE_PATH}/arl/arl.hpp" ]]; then
  echo "Found ARL at ${ARL_SOURCE_PATH}"
else
  echo "Did not find ARL at ${ARL_SOURCE_PATH}!"
  exit 1
fi

# create the ./init directory
mkdir_s ./init
# move to ./init directory
cd init

# setup module environment
export PKG_CONFIG_PATH=$GASNETEX_ROOT/lib/pkgconfig
module purge
module load craype-haswell
module load PrgEnv-gnu
module load cmake
module load upcxx
#export CC=gcc
#export CXX=g++

# record build status
record_env

mkdir -p log
mv *.log log

# build ARL
mkdir -p build
cd build
echo "Running cmake..."
ARL_INSTALL_PATH=$(realpath "../install")
cmake -DCMAKE_INSTALL_PREFIX=${ARL_INSTALL_PATH} \
      -DCMAKE_BUILD_TYPE=Release \
      -L \
      ${ARL_SOURCE_PATH} | tee init-cmake.log 2>&1 || { echo "cmake error!"; exit 1; }
cmake -LAH . >> init-cmake.log
echo "Running make..."
make VERBOSE=1 -j | tee init-make.log 2>&1 || { echo "make error!"; exit 1; }
#echo "Installing ARL to ${ARL_INSTALL_PATH}"
#make install > init-install.log 2>&1 || { echo "install error!"; exit 1; }
mv *.log ../log