
@echo off
SET platform_reverse_path=%1
SET platform_script_name=arduino_due_build_and_run.bat
SET platform_script=%platform_reverse_path%%platform_script_name% 
call %platform_script%

@echo off
SET platform_reverse_path=%1
SET platform_script_name=nrf52_build_and_run.bat
SET platform_script=%platform_reverse_path%%platform_script_name% 
call %platform_script%

@echo off
SET platform_reverse_path=%1
SET platform_script_name=nrf52840_build_and_run.bat
SET platform_script=%platform_reverse_path%%platform_script_name% 
call %platform_script%

@echo off
SET platform_reverse_path=%1
SET platform_script_name=nrf9160_build_and_run.bat
SET platform_script=%platform_reverse_path%%platform_script_name% 
call %platform_script%

@echo off
SET platform_reverse_path=%1
SET platform_script_name=freedom_build_and_run.bat
SET platform_script=%platform_reverse_path%%platform_script_name% 
call %platform_script%
