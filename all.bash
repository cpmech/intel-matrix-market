#!/bin/bash

set -e

SOURCE=`pwd`

cd /tmp
mkdir -p build-intel-matrix-market
cd build-intel-matrix-market/
cmake -S $SOURCE 
make && ./solve_matrix_market
