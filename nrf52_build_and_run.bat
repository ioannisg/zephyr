rmdir /Q /S build_nrf52
mkdir build_nrf52
cd build_nrf52
cmake -GNinja -DBOARD=nrf52_pca10040 ..
ninja
west flash --snr 682672451
cd ..
rmdir /Q /S build_nrf52