@echo off
cd /d %~dp0
SET Include=-I Include -I ../../btrlib/shader/include 
SET output=binary

glslc.exe -w %Include%	-o binary/CopyImage.vert.spv CopyImage.vert
glslc.exe -w %Include%	-o binary/CopyImage.frag.spv CopyImage.frag

