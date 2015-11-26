./autogen.sh
./configure --enable-python --prefix=$PREFIX
make
make install
cd python
make
make install
python setup.py install
