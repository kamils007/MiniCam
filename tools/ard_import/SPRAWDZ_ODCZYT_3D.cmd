@echo off
setlocal
set "MINICAM_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "MINICAM_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%MINICAM_PS%" -NoProfile -STA -File "%~dp0ReadTranslatorMetadata.ps1" %*
set "MINICAM_RESULT=%ERRORLEVEL%"
echo.
echo Przeslij plik translator_metadata.json z nowego folderu wynik_...
pause
exit /b %MINICAM_RESULT%
