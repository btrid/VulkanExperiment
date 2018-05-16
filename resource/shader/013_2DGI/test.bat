@echo off
cd /d %~dp0
SET include=-I ../include

glslangValidator.exe %include% --target-env vulkan1.0 -V -o a.comp.spr a.comp