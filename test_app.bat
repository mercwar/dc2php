@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"
cls
echo =========================================================================
echo [TEST RUNNER] Initializing DC2PHP Application & Session Variable Test
echo =========================================================================
echo [RUNNER] Executing: dc2php.exe -json tests_app.json
echo -------------------------------------------------------------------------

dc2php.exe -json test_app.json

echo -------------------------------------------------------------------------
echo [VERIFICATION] metrics.json output detected:
if exist metrics.json (
    type metrics.json
) else (
    echo [ERROR] metrics.json not found!
)
echo.
echo =========================================================================
echo [TEST RUNNER] Application and session test completed successfully.
echo =========================================================================
pause