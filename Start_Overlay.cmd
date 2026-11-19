@echo off
setlocal
cd /d "%~dp0"
title Brooklyn Overlay Builder
echo ================================================================
echo BROOKLYN OVERLAY BUILDER
echo ================================================================
echo Checking Python and the included build inputs...
echo The working project will open in EasyEDA.
echo Open its PCB drawing, then install/configure the extension before clicking OK in the setup checklist.
echo Keep this window open. The Build map URL appears after you confirm setup.
echo On success, the upload ZIP is also copied beside this CMD file.
echo.
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
