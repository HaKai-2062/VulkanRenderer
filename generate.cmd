@echo off
set out_dir=%CD%\out

if not exist "%out_dir%" mkdir "%out_dir%"
cd /d "%out_dir%"
cmake .. -Wno-dev
pause