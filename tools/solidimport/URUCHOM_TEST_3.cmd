@echo off
setlocal
cd /d "%~dp0"
rem Find per-user Python even when this window inherited an old PATH.
set "SOLIDIMPORT_INIT_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined SOLIDIMPORT_INIT_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "SOLIDIMPORT_INIT_PYTHON=%%~fD\python.exe"
    )
)
if defined SOLIDIMPORT_INIT_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0test_solidimport_init.py" %*
goto finish
:run_direct
"%SOLIDIMPORT_INIT_PYTHON%" "%~dp0test_solidimport_init.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0test_solidimport_init.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona. Przeslij ten komunikat.
echo Raport testu nie powstal.
pause
exit /b 1
:finish
set "SOLIDIMPORT_INIT_EXIT=%ERRORLEVEL%"
echo.
if not "%SOLIDIMPORT_INIT_EXIT%"=="0" goto probe_error
echo Przeslij plik ZIP solidimport_test_3_... utworzony obok folderu wynikow.
pause
exit /b 0
:probe_error
echo Proba zglosila blad lub niepotwierdzony wynik. Przeslij raport i logi z nowego folderu.
pause
exit /b %SOLIDIMPORT_INIT_EXIT%
