@echo off
if /i "%~1"=="--run" goto run
"%ComSpec%" /d /k ""%~f0" --run"
exit /b
:run
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
call py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3,12) else 1)"
if errorlevel 1 goto python_needed
call py -3 tools\overlay\launch.py --bootstrap
set "brooklyn_result=%errorlevel%"
goto finished
:use_python
call python -c "import sys; sys.exit(0 if sys.version_info >= (3,12) else 1)"
if errorlevel 1 goto python_needed
call python tools\overlay\launch.py --bootstrap
set "brooklyn_result=%errorlevel%"
goto finished
:python_needed
set "brooklyn_result=1"
echo Install Python 3.12 or newer from https://www.python.org/downloads/windows/
echo Then double-click Start_Overlay.cmd again.
:finished
echo.
echo ================================================================
echo Launcher finished with exit code %brooklyn_result%.
echo If a problem occurred, read Overlay-launch.log beside this CMD file.
echo This window stays open. Close it with X or type exit when finished.
echo ================================================================
exit /b %brooklyn_result%
