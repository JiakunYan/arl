#!/usr/bin/env bash

package_name=GASNet-2021.3.0-release
wget "https://gasnet.lbl.gov/EX/GASNet-2021.3.0.tar.gz"
tar -xf GASNet-2021.3.0.tar.gz
mv GASNet-2021.3.0 GASNet-2021.3.0-source
mkdir GASNet-2021.3.0-build
mkdir $package_name
gasnet_prefix=$(readlink -f $package_name)
cd GASNet-2021.3.0-source
cp other/contrib/cross-configure-cray-aries-slurm .
cd ../GASNet-2021.3.0-build
../GASNet-2021.3.0-source/configure --prefix=$gasnet_prefix
make -j
make install
cd ..
rm -r GASNet-2021.3.0-build GASNet-2021.3.0-source GASNet-2021.3.0.tar.gz