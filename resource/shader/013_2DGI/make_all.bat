@echo off

call make.bat
cd /d %~dp0

call crowd/make.bat
cd /d %~dp0
call path/make.bat
cd /d %~dp0
call sdf/make.bat
cd /d %~dp0
call rigidbody/make.bat
cd /d %~dp0
call radiosity2/make2.bat
cd /d %~dp0

call voronoi/make.bat
cd /d %~dp0
