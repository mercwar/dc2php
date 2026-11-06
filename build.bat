@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"

title DC2PHP MASTER COMPILATION ENGINE
color 0E

echo [DC2PHP] Probing for standard MSVC x64 build environment toolchains...
set "MSVC_ENV="

:: Check common MSVC 2022/2019 installation locations across Editions
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "MSVC_ENV=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "MSVC_ENV=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "MSVC_ENV=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "MSVC_ENV=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "MSVC_ENV=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

:: Fallback check via vswhere if standard paths miss
if not defined MSVC_ENV (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            set "MSVC_ENV=%%i\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if not defined MSVC_ENV (
    echo [CRITICAL] MSVC x64 Native compiler toolset batch not detected via standard paths or vswhere.
    pause
    exit /b 1
)

echo [DC2PHP] Initializing MSVC x64 Environment...
call "%MSVC_ENV%" >nul

if not exist obj mkdir obj

echo =========================================================================
echo [BUILD] Compiling Win32 resource metadata script...
echo =========================================================================
rc.exe /fo obj\resource.res resource.rc
if errorlevel 1 goto :BUILD_FAIL

echo =========================================================================
echo [BUILD] Compiling and Linking Standalone Wrapper with cJSON  Tracker
echo =========================================================================

cl ^
  /nologo /O2 /Gd /Zp8 /MD ^
  main.c DX-TRACK\Tracker.c cJSON\cJSON.c obj\resource.res ^
  /Fo:obj\\ /Fe:dc2php.exe ^
  /I. /I"cJSON" /I"DX-TRACK" ^
  /D _CRT_SECURE_NO_WARNINGS ^
  /link /SUBSYSTEM:CONSOLE user32.lib advapi32.lib kernel32.lib

if errorlevel 1 goto :BUILD_FAIL

echo.
echo [SUCCESS] Standalone wrapper binary compiled safely!
echo [SUCCESS] Output: %~dp0dc2php.exe
echo =========================================================================
pause
exit /b 0

:BUILD_FAIL
echo.
echo [CRITICAL] MSVC Compilation or Linking pipeline broke down.
pause
exit /b 1