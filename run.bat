@echo off
:: Force close shop.exe if it's already running in the background
taskkill /f /im shop.exe >nul 2>&1

g++ -std=c++11 -Iinclude -o shop src\*.cpp
if %errorlevel% equ 0 (
    echo Compilation successful! Running shop.exe...
    echo ----------------------------------------
    shop.exe
) else (
    echo Compilation failed!
)
pause
