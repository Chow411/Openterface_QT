@echo off
REM ============================================================================
REM Shared FFmpeg Build Script for Windows (external MinGW; package-managed support removed)
REM ============================================================================
REM This script builds FFmpeg with shared libraries (DLLs) for Windows using an
REM external MinGW toolchain (e.g., C:\mingw64) and a bash shell (Git Bash).
REM Automated package-managed installation/support was removed.
REM ============================================================================

setlocal enabledelayedexpansion

REM Configuration
set FFMPEG_VERSION=6.1.1
set LIBJPEG_TURBO_VERSION=3.0.4
set FFMPEG_INSTALL_PREFIX=C:\ffmpeg-shared
set BUILD_DIR=%cd%\ffmpeg-build-temp
set SCRIPT_DIR=%~dp0

REM Optional environment variables:
REM   EXTERNAL_MINGW=C:\mingw64  -> Use this external MinGW toolchain (script will pass the path to bash as SKIP_MSYS_MINGW=1)
REM   ENABLE_NVENC=1              -> Attempt to enable NVENC support (requires NVENC SDK/headers); default is disabled
REM   NVENC_SDK_PATH=...          -> Path to NVENC SDK when ENABLE_NVENC=1 (optional)


REM Colors for output (Windows console codes)
echo [92m============================================================================[0m
echo [92mFFmpeg Shared Build Script for Windows[0m
echo [92m============================================================================[0m

echo [92mThis wrapper prefers using a bash on PATH (e.g., Git Bash) and an external MinGW (EXTERNAL_MINGW). Set SKIP_MSYS_MINGW=1 to prefer an external toolchain.[0m

REM Check if build script exists
if not exist "%SCRIPT_DIR%build-shared-ffmpeg-windows.sh" (
    echo [91mError: Build script not found at %SCRIPT_DIR%build-shared-ffmpeg-windows.sh[0m
    exit /b 1
)

REM Convert Windows path to POSIX-style path (C:\path\to -> /c/path/to)
set SCRIPT_PATH=%SCRIPT_DIR%build-shared-ffmpeg-windows.sh
set SCRIPT_PATH_MSYS=%SCRIPT_PATH:\=/%
set SCRIPT_PATH_MSYS=%SCRIPT_PATH_MSYS::=%
set SCRIPT_PATH_MSYS=/%SCRIPT_PATH_MSYS%

echo [92mLaunching bash and external MinGW environment for shared build (using external toolchain)...[0m
echo [93mThis will take some time (30-60 minutes depending on your system)[0m
echo.

REM If EXTERNAL_MINGW is provided, convert it to POSIX-style path and pass it into the bash invocation
REM Prefer using a bash on PATH (e.g., Git Bash).
if defined EXTERNAL_MINGW (
    set EXTM=%EXTERNAL_MINGW%
    set EXTM_MSYS=%EXTM:\=/%
    set EXTM_MSYS=%EXTM_MSYS::=%
    set EXTM_MSYS=/%EXTM_MSYS%
    echo [93mUsing external MinGW: %EXTERNAL_MINGW% (msys path: %EXTM_MSYS%)[0m
    where bash >nul 2>&1
    if %errorlevel% equ 0 (
        echo [92mFound bash on PATH - invoking shared build script with external MinGW[0m
        REM Pass environment variables through to the bash command so the script can pick them up
        bash -lc "EXTERNAL_MINGW_MSYS=%EXTM_MSYS% SKIP_MSYS_MINGW=1 bash '%SCRIPT_PATH_MSYS%'"
    ) else (
        echo [91mError: bash.exe not found on PATH. Install Git for Windows or ensure bash is available.[0m
        exit /b 1
    )
) else (
    REM No EXTERNAL_MINGW specified — attempt to run the script with bash on PATH
    where bash >nul 2>&1
    if %errorlevel% equ 0 (
        echo [92mFound bash on PATH - invoking shared build script using system bash[0m
        bash -lc "bash '%SCRIPT_PATH_MSYS%'"
    ) else (
        echo [91mError: bash.exe not found on PATH and EXTERNAL_MINGW not set; please install Git for Windows (bash) or set EXTERNAL_MINGW to use an external MinGW.[0m
        exit /b 1
    )
)

if %errorlevel% equ 0 (
    echo.
    echo [92m============================================================================[0m
    echo [92mFFmpeg shared build completed successfully![0m
    echo [92m============================================================================[0m
    echo Installation directory: %FFMPEG_INSTALL_PREFIX%
    echo.
    echo To use this FFmpeg build in your project, set the environment variable:
    echo   set FFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX%
    echo.
    echo Or in CMake:
    echo   cmake -DFFMPEG_PREFIX=%FFMPEG_INSTALL_PREFIX% ...
    echo [92m============================================================================[0m
) else (
    echo.
    echo [91m============================================================================[0m
    echo [91mFFmpeg shared build failed![0m
    echo [91m============================================================================[0m
    echo Check the output above for errors
    echo [91m============================================================================[0m
    exit /b 1
)

exit /b 0
