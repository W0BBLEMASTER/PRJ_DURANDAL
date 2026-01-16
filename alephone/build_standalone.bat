@echo off
set "startTime=%time%"
echo [*] Starting BUILD_STANDALONE (Honest Mode: -j4 -O2)...

:: Call the WSL build script without silencing output
wsl -e make -f Makefile.libretro -j4

:: Check the exit code of the last command
if %ERRORLEVEL% NEQ 0 (
    echo [!] ERROR: BUILD FAILED.
    goto :FAILED
)

echo [*] Copying to Windows lib folder...
copy durandal_libretro.dll ..\lib\durandal_libretro.dll /Y

echo [!] BUILD_COMPLETE
set "endTime=%time%"
echo Started: %startTime%
echo Finished: %endTime%
pause
exit /b 0

:FAILED
echo [!] ERROR: BUILD FAILED
pause
exit /b 1