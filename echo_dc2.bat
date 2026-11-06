@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"

:: ---------------------------------------------------------------------------
:: DC2PHP Command-Line Orchestrator Wrapper (`run_dc2php.bat`)
:: ---------------------------------------------------------------------------

set "TARGET_TEST=1"
set "WORKER_COUNT=5"
set "PHP_SCRIPT=echo_dc2.php"

:: Parse incoming command-line parameters (--test=X, --workers=Y, --script=Z)
for %%A in (%*) do (
    set "ARG=%%A"
    if "!ARG:~0,7!"=="--test=" set "TARGET_TEST=!ARG:~7!"
    if "!ARG:~0,10!"=="--workers=" set "WORKER_COUNT=!ARG:~10!"
    if "!ARG:~0,9!"=="--script=" set "PHP_SCRIPT=!ARG:~9!"
)

echo [BATCH] ========================================================
echo [BATCH] Initializing DC2PHP CLI Orchestration Wrapper...
echo [BATCH] Test ID      : !TARGET_TEST!
echo [BATCH] Worker Count : !WORKER_COUNT!
echo [BATCH] Target Script: !PHP_SCRIPT!
echo [BATCH] ========================================================

:: 1. Verify existence of the compiled C binary and PHP payload
if not exist "dc2php.exe" (
    echo [CRITICAL] dc2php.exe not found in working directory. Run compilation first.
    pause
    exit /b 1
)

if not exist "!PHP_SCRIPT!" (
    echo [CRITICAL] Target PHP script '!PHP_SCRIPT!' not found.
    pause
    exit /b 1
)

echo.
echo [BATCH] Step 1: Executing compiled C test matrix runner...
dc2php.exe --test=!TARGET_TEST! --workers=!WORKER_COUNT!
if errorlevel 1 (
    echo [FAIL] C binary test execution encountered an error.
    pause
    exit /b 1
)

echo.
echo [BATCH] Step 2: Dispatching payload to embedded PHP SAPI runner...
echo [BATCH] --------------------------------------------------------

:: Execute dc2php passing the target php file to evaluate and echo response
dc2php.exe !PHP_SCRIPT!

echo [BATCH] --------------------------------------------------------
echo [BATCH] [SUCCESS] DC2PHP execution pipeline completed.
pause
exit /b 0