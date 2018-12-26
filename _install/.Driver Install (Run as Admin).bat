@echo off

devcon.exe remove Nefarius\ViGEmBus\Gen1

echo Installing the ViGEm emulated gamepads driver..

call devcon.exe install ..\bin\x64\ViGEmBus\ViGEmBus.inf Nefarius\ViGEmBus\Gen1
if %ERRORLEVEL% == 0 goto :success
echo Installation failed; you need to right click the .bat and "run as admin".
pause 
exit

:success
echo Installation is successful
echo Done

ECHO.
ECHO.
pause
