rmdir /Q /S build_arduino
mkdir build_arduino
cd build_arduino
cmake -GNinja -DBOARD=arduino_due ..
ninja
echo h > commanderscript.bat
echo loadfile zephyr\zephyr.bin >> commanderscript.bat
echo r0 >> commanderscript.bat
echo r1 >> commanderscript.bat
echo exit >> commanderscript.bat
JLink -device ATSAM3X8E -if SWD -speed 4000 -SelectEmuBySN 28011547 -autoconnect 1 -CommanderSCript commanderscript.bat
cd ..
rmdir /Q /S build_arduino