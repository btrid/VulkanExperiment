@echo off

call make.bat
cd /d %~dp0

call fluid/make.bat
cd /d %~dp0
call crowd/make.bat
cd /d %~dp0
call path/make.bat
cd /d %~dp0
call rigidbody_dem/make.bat
cd /d %~dp0
