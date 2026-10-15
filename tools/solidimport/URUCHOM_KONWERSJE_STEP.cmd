@echo off
setlocal
cd /d "%~dp0"
rem Find per-user Python even when this window inherited an old PATH.
set "SOLIDLINK_STEP_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined SOLIDLINK_STEP_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "SOLIDLINK_STEP_PYTHON=%%~fD\python.exe"
    )
)
if defined SOLIDLINK_STEP_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0test_step_conversion.py" %*
goto finish
:run_direct
"%SOLIDLINK_STEP_PYTHON%" "%~dp0test_step_conversion.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0test_step_conversion.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona. Przeslij ten komunikat.
echo Raport testu nie powstal.
pause
exit /b 1
:finish
set "SOLIDLINK_STEP_EXIT=%ERRORLEVEL%"
echo.
if not "%SOLIDLINK_STEP_EXIT%"=="0" goto probe_error
echo Przeslij solidlink_step_result.json i converted.x_t z nowego folderu solidlink_test_2_...
pause
exit /b 0
:probe_error
echo Proba zglosila blad lub niepotwierdzony wynik. Przeslij raport i logi z nowego folderu.
pause
exit /b %SOLIDLINK_STEP_EXIT%
