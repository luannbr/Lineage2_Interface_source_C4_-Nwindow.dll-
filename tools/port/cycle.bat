@echo off
rem the C3 cycle for the tree given as the first argument (default: this one): build, smoke, verify, regcheck
rem usage: tools\port\cycle.bat [root]   (run from any shell; it sets up the VS2022 x86 environment itself)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
if "%~1"=="" (cd /d "%~dp0\..\..") else (cd /d "%~1")
python tools\cycle.py --game-source --no-install
if errorlevel 1 exit /b 1
python tools\port\verify_data.py
if errorlevel 1 exit /b 1
python tools\port\check_breakers.py
if errorlevel 1 exit /b 1
python tools\port\checks.py
