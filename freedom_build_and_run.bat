rmdir /Q /S build_freedom
mkdir build_freedom
cd build_freedom
cmake -GNinja -DBOARD=frdm_k64f ..
ninja flash
cd ..
rmdir /Q /S build_freedom
