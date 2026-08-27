@echo off
g++ -std=c++11 -o shop3d main.cpp
if %errorlevel% equ 0 (
    echo Compilation successful! Running shop3d.exe...
    echo ----------------------------------------
    shop3d.exe
) else (
    echo Compilation failed!
)
pause