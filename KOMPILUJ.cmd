@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows\Build.ps1" %*
set "MINICAM_RESULT=%ERRORLEVEL%"
if not "%MINICAM_NO_PAUSE%"=="1" pause
exit /b %MINICAM_RESULT%
