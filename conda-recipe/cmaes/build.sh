export CFLAGS=-I$($PYTHON -c 'import numpy as np;print(np.get_include())')
export LDFLAGS=-L"$PREFIX/lib"
./autogen.sh
./configure --enable-python --prefix=$PREFIX
make -j20
make install
cd python
make
make install
$PYTHON setup.py install
