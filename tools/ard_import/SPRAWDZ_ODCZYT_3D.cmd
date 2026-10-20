@echo off
setlocal
set "MINICAM_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "MINICAM_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
rem Execution permission applies only to this child process, not the machine or user policy.
"%MINICAM_PS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0ReadTranslatorMetadata.ps1" %*
set "MINICAM_RESULT=%ERRORLEVEL%"
echo.
if not "%MINICAM_RESULT%"=="0" goto failed
echo Przeslij plik translator_metadata.json ze wskazanego folderu wynik_...
goto finish
:failed
echo Skrypt nie zakonczyl sie poprawnie. Raport mogl nie powstac.
echo Przeslij komunikat bledu widoczny powyzej.
:finish
pause
exit /b %MINICAM_RESULT%
