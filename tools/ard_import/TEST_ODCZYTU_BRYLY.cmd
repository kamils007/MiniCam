@echo off
setlocal
set "MINICAM_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "MINICAM_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%MINICAM_PS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0InvokeArdRead.ps1" %*
set "MINICAM_RESULT=%ERRORLEVEL%"
echo.
if not "%MINICAM_RESULT%"=="0" echo Test nie potwierdzil odczytu bryly. Przeslij raport lub komunikat powyzej.
pause
exit /b %MINICAM_RESULT%
