#!/usr/bin/env bash

package_name=upcxx-2021.3.0-release
wget "https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-2021.3.0.tar.gz"
tar -xf upcxx-2021.3.0.tar.gz
mv upcxx-2021.3.0 upcxx-2021.3.0-source
mkdir upcxx-2021.3.0-build
mkdir $package_name
upcxx_prefix=$(readlink -f $package_name)
cd upcxx-2021.3.0-build
../upcxx-2021.3.0-source/configure --prefix=$upcxx_prefix
make
make install
cd ..
rm -r upcxx-2021.3.0-build upcxx-2021.3.0-source upcxx-2021.3.0.tar.gz