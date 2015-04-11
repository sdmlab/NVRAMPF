
sudo apt-get install build-essential libboost-all-dev

wget http://llvm.org/releases/3.5.1/llvm-3.5.1.src.tar.xz
wget http://llvm.org/releases/3.5.1/cfe-3.5.1.src.tar.xz
wget http://llvm.org/releases/3.5.1/clang-tools-extra-3.5.1.src.tar.xz
wget http://llvm.org/releases/3.5.1/compiler-rt-3.5.1.src.tar.xz

tar -xf llvm-3.5.1.src.tar.xz
tar -xf cfe-3.5.1.src.tar.xz 
tar -xf clang-tools-extra-3.5.1.src.tar.xz 
tar -xf compiler-rt-3.5.1.src.tar.xz 

mv cfe-3.5.1.src llvm-3.5.1.src/tools/clang
mv clang-tools-extra-3.5.1.src llvm-3.5.1.src/tools/clang/tools/extra
mv compiler-rt-3.5.1.src llvm-3.5.1.src/projects/compiler-rt

mkdir build
cd build
../llvm-3.5.1.src/configure --enable-optimized 

make 
sudo make install

