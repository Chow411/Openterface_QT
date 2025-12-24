@echo off
REM ============================================================================
REM Shared FFmpeg Build Script for Windows (external MinGW)
REM ============================================================================
REM This script builds FFmpeg with shared libraries (DLLs) for Windows using an
REM external MinGW toolchain (e.g., C:\mingw64) and a bash shell (Git Bash).
REM ============================================================================

setlocal enabledelayedexpansion

REM Configuration
set "FFMPEG_VERSION=6.1.1"
set "LIBJPEG_TURBO_VERSION=3.0.4"
set "FFMPEG_INSTALL_PREFIX=C:\ffmpeg-shared"
set "BUILD_DIR=%cd%\ffmpeg-build-temp"
set "SCRIPT_DIR=%~dp0"

REM Optional environment variables:
REM   EXTERNAL_MINGW=C:\mingw64  -> External MinGW toolchain path
REM   ENABLE_NVENC=1              -> Enable NVENC support (requires NVENC SDK)
REM   NVENC_SDK_PATH=...          -> Path to NVENC SDK (optional)

echo ============================================================================
echo FFmpeg Shared Build Script for Windows
echo ============================================================================
echo.

REM Check if build script exists
if not exist "%SCRIPT_DIR%build-shared-ffmpeg-windows.sh" (
    echo ERROR: Build script not found at %SCRIPT_DIR%build-shared-ffmpeg-windows.sh
    exit /b 1
)

REM Check if bash is available
where bash >nul 2>&1
if errorlevel 1 (
    echo ERROR: bash.exe not found on PATH. 
    echo Please install Git for Windows or ensure bash is available.
    exit /b 1
)

echo Found bash: 
where bash | findstr /n "^"
echo.

REM Convert bash script path to POSIX format (C:\path\to -> /c/path/to)
set "SCRIPT_PATH=%SCRIPT_DIR%build-shared-ffmpeg-windows.sh"
call :ConvertToPosix "%SCRIPT_PATH%" SCRIPT_PATH_POSIX

echo Build script: %SCRIPT_PATH%
echo Build script (POSIX): %SCRIPT_PATH_POSIX%
echo.

REM If EXTERNAL_MINGW is set, convert it to POSIX and pass to bash script
if defined EXTERNAL_MINGW (
    REM Remove trailing backslash if present
    set "MINGW_PATH=%EXTERNAL_MINGW%"
    if "!MINGW_PATH:~-1!"=="\" set "MINGW_PATH=!MINGW_PATH:~0,-1!"
    
    REM Convert to POSIX format
    call :ConvertToPosix "!MINGW_PATH!" MINGW_PATH_POSIX
    
    echo Using external MinGW: !MINGW_PATH!
    echo MinGW path (POSIX): !MINGW_PATH_POSIX!
    echo.
    
    REM Create temporary wrapper script
    set "TEMP_WRAPPER=%TEMP%\ffmpeg_build_%RANDOM%.sh"
    (
        echo export EXTERNAL_MINGW_MSYS=!MINGW_PATH_POSIX!
        echo export SKIP_MSYS_MINGW=1
        echo bash "%SCRIPT_PATH_POSIX%"
    ) > "!TEMP_WRAPPER!"
    
    echo Wrapper script created: !TEMP_WRAPPER!
    echo Contents:
    type "!TEMP_WRAPPER!"
    echo.
    echo Starting FFmpeg build (this will take 30-60 minutes)...
    echo.
    
    bash "!TEMP_WRAPPER!"
    set BUILD_EXIT=!errorlevel!
    
    REM Cleanup
    if exist "!TEMP_WRAPPER!" del /f /q "!TEMP_WRAPPER!" >nul 2>&1
    
    if !BUILD_EXIT! neq 0 (
        echo.
        echo ============================================================================
        echo FFmpeg build FAILED!
        echo ============================================================================
        exit /b !BUILD_EXIT!
    )
) else (
    echo No EXTERNAL_MINGW set - using system MinGW
    echo.
    echo Starting FFmpeg build (this will take 30-60 minutes)...
    echo.
    
    bash "%SCRIPT_PATH_POSIX%"
    set BUILD_EXIT=!errorlevel!
    
    if !BUILD_EXIT! neq 0 (
        echo.
        echo ============================================================================
        echo FFmpeg build FAILED!
        echo ============================================================================
        exit /b !BUILD_EXIT!
    )
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

REM ============================================================================
REM Function: Convert Windows path to POSIX format
REM Usage: call :ConvertToPosix "C:\path\to\file" OUTPUT_VAR
REM ============================================================================
:ConvertToPosix
setlocal
set "WIN_PATH=%~1"
set "POSIX_PATH=%WIN_PATH:\=/%"
REM Extract drive letter (e.g., C:)
set "DRIVE=%POSIX_PATH:~0,1%"
REM Remove colon and convert to lowercase
set "POSIX_PATH=/%DRIVE%%POSIX_PATH:~2%"
REM Convert drive letter to lowercase
for %%i in (a b c d e f g h i j k l m n o p q r s t u v w x y z) do (
    call set "POSIX_PATH=%%POSIX_PATH:/%%i=/%%i%%"
)
endlocal & set "%~2=%POSIX_PATH%"
goto :eof
