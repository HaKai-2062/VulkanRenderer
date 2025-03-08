@echo off

if exist "out" (rmdir /s /q "out" & echo Deleting out)

setlocal enabledelayedexpansion
cd vendor/
set "keep_dir=CMake"

for /d %%D in (*) do (
    if /I not "%%D"=="%keep_dir%" (
        echo Deleting: %%D
        rmdir /s /q "%%D"
    )
)

echo.
echo Done! Press any key to exit.
pause