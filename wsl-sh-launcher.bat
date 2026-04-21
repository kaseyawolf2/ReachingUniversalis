@echo off
wsl.exe -e bash -lic "cd \"$(wslpath -u '%~dp1')\" && bash \"$(wslpath -u '%~1')\"; echo EXIT=$?"
pause
