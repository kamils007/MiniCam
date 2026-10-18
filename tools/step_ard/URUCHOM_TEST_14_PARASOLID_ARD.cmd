@echo off
setlocal
cd /d "%~dp0"
set "MINICAM_TEST14_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined MINICAM_TEST14_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "MINICAM_TEST14_PYTHON=%%~fD\python.exe"
    )
)
if defined MINICAM_TEST14_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0build_step_ard_14.py" %*
goto finish
:run_direct
"%MINICAM_TEST14_PYTHON%" "%~dp0build_step_ard_14.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0build_step_ard_14.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona x64. Przeslij ten komunikat.
pause
exit /b 1
:finish
set "MINICAM_TEST14_EXIT=%ERRORLEVEL%"
echo.
if not "%MINICAM_TEST14_EXIT%"=="0" goto probe_error
echo ARD utworzony. Teraz otworz STEP_TEST_14.ard i sprawdz pokrycie z oryginalnym STEP-em.
echo Przeslij wynikowy ZIP oraz STEP_TEST_14_po_otwarciu.ard.
pause
exit /b 0
:probe_error
echo Proba zglosila blad lub niepotwierdzony wynik. Przeslij ZIP z raportem i logami.
pause
exit /b %MINICAM_TEST14_EXIT%
