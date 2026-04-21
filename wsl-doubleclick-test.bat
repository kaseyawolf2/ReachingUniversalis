@echo off
echo === Windows-side launcher ===
echo CWD: %CD%
echo Script dir: %~dp0
echo.
echo Calling wsl.exe...
wsl.exe -e bash -lic "cd \"$(wslpath -u '%~dp0')\" && ./wsl-doubleclick-test.sh; echo EXIT=$?"
echo.
echo wsl.exe returned %ERRORLEVEL%
pause
