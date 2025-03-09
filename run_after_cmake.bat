@echo off
set out_dir=%CD%\out

if not exist "%out_dir%" echo ERROR: glslangValidator not found! Make sure Vulkan SDK is installed.
cd /d "%out_dir%"
cmake --build .
pause