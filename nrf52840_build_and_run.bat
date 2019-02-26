rmdir /Q /S build_nrf52840
mkdir build_nrf52840
cd build_nrf52840
cmake -GNinja -DBOARD=nrf52840_pca10056 ..
ninja
west flash --snr 683446709
cd ..
rmdir /Q /S build_nrf52840