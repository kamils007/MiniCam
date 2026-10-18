@echo off
setlocal
cd /d "%~dp0"
rem Find per-user Python even when this window inherited an old PATH.
set "SOLIDIMPORT_STANDALONE_PYTHON="
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if not defined SOLIDIMPORT_STANDALONE_PYTHON if exist "%%~fD\python.exe" (
        "%%~fD\python.exe" -c "import struct,sys;sys.exit(0 if struct.calcsize('P') == 8 and sys.version_info[0] == 3 else 1)" >nul 2>&1
        if not errorlevel 1 set "SOLIDIMPORT_STANDALONE_PYTHON=%%~fD\python.exe"
    )
)
if defined SOLIDIMPORT_STANDALONE_PYTHON goto run_direct
py -3 --version >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0read_standalone_dependencies.py" %*
goto finish
:run_direct
"%SOLIDIMPORT_STANDALONE_PYTHON%" "%~dp0read_standalone_dependencies.py" %*
goto finish
:try_python
python --version >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0read_standalone_dependencies.py" %*
goto finish
:missing_python
echo Nie udalo sie odnalezc Pythona. Przeslij ten komunikat.
echo Raport testu nie powstal.
pause
exit /b 1
:finish
set "SOLIDIMPORT_STANDALONE_EXIT=%ERRORLEVEL%"
echo.
if not "%SOLIDIMPORT_STANDALONE_EXIT%"=="0" goto probe_error
echo Odczyt zaleznosci zakonczony. Konwersji STEP jeszcze nie wykonywano.
echo Przeslij plik ZIP standalone_test_5_... utworzony obok folderu wynikow.
pause
exit /b 0
:probe_error
echo Proba zglosila blad lub niepotwierdzony wynik. Przeslij raport i logi z nowego folderu.
pause
exit /b %SOLIDIMPORT_STANDALONE_EXIT%
