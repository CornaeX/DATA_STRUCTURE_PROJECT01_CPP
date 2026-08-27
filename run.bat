@echo off
:: Force close shop3d.exe if it's already running in the background
taskkill /f /im shop3d.exe >nul 2>&1

g++ -std=c++11 -o shop3d main.cpp
if %errorlevel% equ 0 (
    echo Compilation successful! Running shop3d.exe...
    echo ----------------------------------------
    shop3d.exe
) else (
    echo Compilation failed!
)
pause