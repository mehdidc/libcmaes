export CFLAGS=-I$($PYTHON -c 'import numpy as np;print(np.get_include())')
#export LDFLAGS=-L/root/miniconda/pkgs/boost-1.57.0-4/lib
./autogen.sh
./configure --enable-python --prefix=$PREFIX
make -j20
make install
cd python
make
make install
$PYTHON setup.py install
