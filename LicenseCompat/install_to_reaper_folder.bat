@echo off
setlocal
set "TARGET=%ProgramFiles%\REAPER (x64)\DrizzleLicenseHost"
echo Installing Drizzle license host to:
echo   %TARGET%\reaper.exe
echo.
if not exist "%TARGET%" mkdir "%TARGET%"
copy /Y "%~dp0reaper.exe" "%TARGET%\reaper.exe"
if errorlevel 1 (
    echo Failed. Run this script as Administrator.
    pause
    exit /b 1
)
copy /Y "%~dp0README.txt" "%TARGET%\README.txt" >nul 2>&1
echo Done. Launch Drizzle.exe normally; it will restart from the Reaper folder copy.
pause
