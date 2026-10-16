@echo off
setlocal
cd /d "%~dp0"
rem Find per-user Python even when this window inherited an old PATH.
set "FEATURE_TEST_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined FEATURE_TEST_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "FEATURE_TEST_PYTHON=%%~fD\python.exe"
    )
)
if defined FEATURE_TEST_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0probe_feature_2025.py" %*
goto finish
:run_direct
"%FEATURE_TEST_PYTHON%" "%~dp0probe_feature_2025.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0probe_feature_2025.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona. Przeslij ten komunikat.
echo Raport testu nie powstal.
pause
exit /b 1
:finish
set "FEATURE_TEST_EXIT=%ERRORLEVEL%"
echo.
if not "%FEATURE_TEST_EXIT%"=="0" goto probe_error
echo Przeslij feature_probe_2025_host.json oraz feature_object.bin z nowego folderu feature_test_4_9_2025_...
pause
exit /b 0
:probe_error
echo Test nie uruchomil sie poprawnie. Przeslij komunikat z tego okna.
pause
exit /b %FEATURE_TEST_EXIT%
