@echo off
setlocal
cd /d "%~dp0"
echo Brooklyn Overlay Builder
echo Checking Python and the included build inputs...
where py >nul 2>nul
if errorlevel 1 goto use_python
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3,12) else 1)"
if errorlevel 1 goto python_needed
py -3 tools\overlay\start.py --bootstrap
goto finished
:use_python
python -c "import sys; sys.exit(0 if sys.version_info >= (3,12) else 1)"
if errorlevel 1 goto python_needed
python tools\overlay\start.py --bootstrap
goto finished
:python_needed
echo Install Python 3.12 or newer from https://www.python.org/downloads/windows/
echo Then double-click Start_Overlay.cmd again.
:finished
echo.
pause
