@echo off
setlocal
cd /d "%~dp0"
rem Find per-user Python even when this window inherited an old PATH.
set "STEP_ARD_TEST_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined STEP_ARD_TEST_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "STEP_ARD_TEST_PYTHON=%%~fD\python.exe"
    )
)
if defined STEP_ARD_TEST_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0build_step_ard.py" %*
goto finish
:run_direct
"%STEP_ARD_TEST_PYTHON%" "%~dp0build_step_ard.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0build_step_ard.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona. Przeslij ten komunikat.
echo Raport testu nie powstal.
pause
exit /b 1
:finish
set "STEP_ARD_TEST_EXIT=%ERRORLEVEL%"
echo.
if not "%STEP_ARD_TEST_EXIT%"=="0" goto probe_error
echo Przeslij plik ZIP step_ard_test_4_1_... oraz zapisany w ALPHACAM plik STEP_test_po_otwarciu.ard.
pause
exit /b 0
:probe_error
echo Proba zglosila blad lub niepotwierdzony wynik. Przeslij raport i logi z nowego folderu.
pause
exit /b %STEP_ARD_TEST_EXIT%
