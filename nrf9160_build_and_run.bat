rmdir /Q /S build_nrf9160
mkdir build_nrf9160
cd build_nrf9160
cmake -GNinja -DBOARD=nrf9160_pca10090 ..
ninja
west flash --snr 960042806
cd ..
rmdir /Q /S build_nrf9160

mkdir build_nrf9160ns
cd build_nrf9160ns
cmake -GNinja -DBOARD=nrf9160_pca10090ns ..
ninja
west flash --snr 960034859
cd ..
rmdir /Q /S build_nrf9160ns