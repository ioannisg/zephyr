set ZEPHYR_BASE=C:\Users\iogl\Documents\nordic\workspace\zephyr
set ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
set GNUARMEMB_TOOLCHAIN_PATH=C:\gccarmemb

git fetch origin
git rebase origin/master

cd tests

:: ----------------------------------------------------------------------------
:: kernel tests suite

cd kernel

:: ----------------------------------------------------------------------------
:: mem-protect tests suite
cd mem_protect

SET reverse_path=..\..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%

cd mem_protect
call %script% %reverse_path%
cd ..

cd obj_validation
call %script% %reverse_path%
cd ..

cd protection
call %script% %reverse_path%
cd ..

cd stack_random
call %script% %reverse_path%
cd ..

cd stackprot
call %script% %reverse_path%
cd ..

cd syscalls
call %script% %reverse_path%
cd ..

cd userspace
call %script% %reverse_path%
cd ..

cd ..

:: ----------------------------------------------------------------------------
:: kernel/threads tests suite

cd threads
cd dynamic_thread
SET reverse_path=..\..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

cd thread_init
SET reverse_path=..\..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

cd thread_apis
SET reverse_path=..\..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

cd ..

:: ----------------------------------------------------------------------------
:: kernel/fatal tests suite

cd fatal
SET reverse_path=..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

:: ----------------------------------------------------------------------------
:: kernel/mem_slab tests suite

cd mem_slab
cd mslab_threadsafe
SET reverse_path=..\..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

cd ..

cd ..

:: ----------------------------------------------------------------------------
:: lib tests suite

cd lib
cd mem_alloc
SET reverse_path=..\..\..\
SET script_name=build_and_run_scripts.bat
SET script=%reverse_path%%script_name%
call %script% %reverse_path%
cd ..

cd ..

cd ..