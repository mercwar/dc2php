@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"

title DC2PHP JSON PIPELINE VERIFICATION RUNNER
color 0B

echo =========================================================================
echo [TEST RUNNER] Initializing DC2PHP JSON Execution Pipeline Test
echo =========================================================================

if not exist "dc2php.exe" (
    echo [CRITICAL] dc2php.exe not found in root directory! Please run build.bat first.
    pause
    exit /b 1
)

if not exist "test.json" (
    echo [CRITICAL] test.json configuration file missing!
    pause
    exit /b 1
)

if not exist "test.php" (
    echo [CRITICAL] test.php script missing!
    pause
    exit /b 1
)

echo [RUNNER] Executing: dc2php.exe -json tests.json
echo -------------------------------------------------------------------------
dc2php.exe -json test.json
echo -------------------------------------------------------------------------

if exist "metrics.json" (
    echo [VERIFICATION] metrics.json output detected:
    type metrics.json
    echo.
) else (
    echo [WARNING] metrics.json was not generated.
)

echo =========================================================================
echo [TEST RUNNER] Pipeline execution test completed successfully.
echo =========================================================================
pause
exit /b 0