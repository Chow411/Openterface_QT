@echo off
REM ============================================================================
REM Shared FFmpeg Build Script for Windows (external MinGW)
REM This script builds FFmpeg with shared libraries using an external MinGW
REM toolchain and Git Bash.
REM ============================================================================

setlocal enabledelayedexpansion

REM Configuration
set "FFMPEG_VERSION=6.1.1"
set "LIBJPEG_TURBO_VERSION=3.0.4"
set "FFMPEG_INSTALL_PREFIX=C:\ffmpeg-shared"
set "BUILD_DIR=%cd%\ffmpeg-build-temp"
set "SCRIPT_DIR=%~dp0"

REM Optional environment variables:
REM   EXTERNAL_MINGW=C:\mingw64  -> Path to external MinGW toolchain
REM   ENABLE_NVENC=1              -> Enable NVENC support
REM   NVENC_SDK_PATH=...          -> Path to NVENC SDK (optional)

echo ============================================================================
echo FFmpeg Shared Build Script for Windows
echo ============================================================================

REM Locate Git Bash (prefer it over WSL bash)
set "GIT_BASH="
if exist "C:\Program Files\Git\bin\bash.exe" (
    set "GIT_BASH=C:\Program Files\Git\bin\bash.exe"
) else if exist "C:\Program Files (x86)\Git\bin\bash.exe" (
    set "GIT_BASH=C:\Program Files (x86)\Git\bin\bash.exe"
)

if not defined GIT_BASH (
    echo ERROR: Git Bash not found. Please install Git for Windows.
    exit /b 1
)

echo Using Git Bash: !GIT_BASH!

REM Check build script exists
set "SCRIPT_PATH=%SCRIPT_DIR%build-shared-ffmpeg-windows.sh"
if not exist "!SCRIPT_PATH!" (
    echo ERROR: Build script not found at !SCRIPT_PATH!
    exit /b 1
)

echo Build script: !SCRIPT_PATH!

REM Determine MinGW path
if defined EXTERNAL_MINGW (
    set "MINGW_PATH=!EXTERNAL_MINGW!"
    REM Remove trailing backslash if present
    if "!MINGW_PATH:~-1!"=="\" set "MINGW_PATH=!MINGW_PATH:~0,-1!"
) else (
    set "MINGW_PATH=C:\mingw64"
)

REM Validate MinGW toolchain
if not exist "!MINGW_PATH!\bin\gcc.exe" (
    echo ERROR: gcc.exe not found in MinGW directory: !MINGW_PATH!\bin
    echo Please set EXTERNAL_MINGW to a valid MinGW-w64 installation.
    exit /b 1
)

echo Using external MinGW: !MINGW_PATH!

REM Set environment for the bash script
set "EXTERNAL_MINGW_MSYS=!MINGW_PATH!"
set "SKIP_MSYS_MINGW=1"

echo.
echo Starting FFmpeg build (this will take 30-60 minutes)...
echo.

REM Launch the bash script via Git Bash with Windows path
"!GIT_BASH!" "!SCRIPT_PATH!"

set "BUILD_EXIT=!errorlevel!"
if !BUILD_EXIT! neq 0 (
    echo.
    echo ============================================================================
    echo FFmpeg build FAILED!
    echo ============================================================================
    exit /b !BUILD_EXIT!
)

echo.
echo ============================================================================
echo FFmpeg build completed successfully!
echo ============================================================================
echo Installation directory: %FFMPEG_INSTALL_PREFIX%
echo.
echo To use this FFmpeg build, set in your environment:
echo   set FFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX%
echo.
echo Or in CMake:
echo   cmake -DFFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX% ...
echo ============================================================================

exit /b 0