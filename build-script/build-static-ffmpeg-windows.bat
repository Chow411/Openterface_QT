@echo off
REM ============================================================================
REM Static FFmpeg Build Script for Windows (external MinGW; package-managed support removed)
REM ============================================================================
REM This script builds FFmpeg with static libraries for Windows using an
REM external MinGW toolchain (e.g., C:\mingw64) and a bash shell (Git Bash).
REM Automated package-managed installation/support has been removed.
REM ============================================================================

setlocal enabledelayedexpansion

REM Configuration
set FFMPEG_VERSION=6.1.1
set LIBJPEG_TURBO_VERSION=3.0.4
set FFMPEG_INSTALL_PREFIX=C:\ffmpeg-static
set BUILD_DIR=%cd%\ffmpeg-build-temp
set SCRIPT_DIR=%~dp0

REM Optional environment variables:
REM   EXTERNAL_MINGW=C:\mingw64  -> Use this external MinGW toolchain (script will pass the path to bash as SKIP_MSYS_MINGW=1)
REM   ENABLE_NVENC=1              -> Attempt to enable NVENC support (requires NVENC SDK/headers); default is disabled
REM   NVENC_SDK_PATH=...          -> Path to NVENC SDK when ENABLE_NVENC=1 (optional)


REM Colors for output (Windows console codes)
echo ================================================================================
echo FFmpeg Static Build Script for Windows
echo ================================================================================

REM NOTE: This wrapper prefers using a bash on PATH (e.g., Git Bash) and an external MinGW (EXTERNAL_MINGW). Set SKIP_MSYS_MINGW=1 to prefer an external toolchain.

REM Check if build script exists
if not exist "%SCRIPT_DIR%build-static-ffmpeg-windows.sh" (
    echo Error: Build script not found at %SCRIPT_DIR%build-static-ffmpeg-windows.sh
    exit /b 1
)

REM Convert Windows path to POSIX-style path (C:\path\to -> /c/path/to)
set SCRIPT_PATH=%SCRIPT_DIR%build-static-ffmpeg-windows.sh
set SCRIPT_PATH_MSYS=%SCRIPT_PATH:\=/%
set SCRIPT_PATH_MSYS=%SCRIPT_PATH_MSYS::=%
set SCRIPT_PATH_MSYS=/%SCRIPT_PATH_MSYS%

echo Launching bash and external MinGW environment (using external toolchain)...
echo This will take some time (30-60 minutes depending on your system)
echo.

REM If EXTERNAL_MINGW is provided, convert it to POSIX-style path and pass it into the bash invocation
REM Prefer using a bash on PATH (e.g., Git Bash).
if defined EXTERNAL_MINGW (
    set EXTM=%EXTERNAL_MINGW%
    set EXTM_MSYS=%EXTM:\=/%
    set EXTM_MSYS=%EXTM_MSYS::=%
    set EXTM_MSYS=/%EXTM_MSYS%
    echo Using external MinGW: %EXTERNAL_MINGW% (msys path: %EXTM_MSYS%)
    where bash >nul 2>&1
    if %errorlevel% equ 0 (
        echo Found bash on PATH - invoking build script with external MinGW
        REM Pass environment variables through to the bash command so the script can pick them up
        bash -lc "EXTERNAL_MINGW_MSYS=%EXTM_MSYS% SKIP_MSYS_MINGW=1 bash '%SCRIPT_PATH_MSYS%'"
    ) else (
        echo Error: bash.exe not found on PATH. Install Git for Windows or ensure bash is available.
        exit /b 1
    )
) else (
    REM No EXTERNAL_MINGW specified — attempt to run the script with bash on PATH (non-MS Y S usage)
    where bash >nul 2>&1
    if %errorlevel% equ 0 (
        echo Found bash on PATH - invoking build script using system bash
        bash -lc "bash '%SCRIPT_PATH_MSYS%'"
    ) else (
        echo Error: bash.exe not found on PATH and EXTERNAL_MINGW not set; please install Git for Windows (bash) or set EXTERNAL_MINGW to use an external MinGW.
        exit /b 1
    )
)

if %errorlevel% equ 0 (
    echo.
    echo ================================================================================
    echo FFmpeg build completed successfully!
    echo ================================================================================
    echo Installation directory: %FFMPEG_INSTALL_PREFIX%
    echo.
    echo To use this FFmpeg build in your project, set the environment variable:
    echo   set FFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX%
    echo.
    echo Or in CMake:
    echo   cmake -DFFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX% ...
    echo ================================================================================
) else (
    echo.
    echo ================================================================================
    echo FFmpeg build failed!
    echo ================================================================================
    echo Check the output above for errors
    echo ================================================================================
    exit /b 1
)
fi

exit /b 0