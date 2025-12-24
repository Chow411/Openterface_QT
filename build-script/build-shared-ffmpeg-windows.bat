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
    echo Starting FFmpeg build (this will take 30-60 minutes)...
    echo.
    
    REM Set environment variables and run bash script directly
    set EXTERNAL_MINGW_MSYS=!MINGW_PATH_POSIX!
    set SKIP_MSYS_MINGW=1
    
    bash "%SCRIPT_PATH_POSIX%"
    set BUILD_EXIT=!errorlevel!
    
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
setlocal enabledelayedexpansion
set "WIN_PATH=%~1"
REM Replace backslashes with forward slashes
set "POSIX_PATH=!WIN_PATH:\=/!"
REM Extract drive letter and convert to lowercase
set "DRIVE_LETTER=!POSIX_PATH:~0,1!"
REM Map uppercase to lowercase
if /i "!DRIVE_LETTER!"=="A" set "DRIVE_LOWER=a"
if /i "!DRIVE_LETTER!"=="B" set "DRIVE_LOWER=b"
if /i "!DRIVE_LETTER!"=="C" set "DRIVE_LOWER=c"
if /i "!DRIVE_LETTER!"=="D" set "DRIVE_LOWER=d"
if /i "!DRIVE_LETTER!"=="E" set "DRIVE_LOWER=e"
if /i "!DRIVE_LETTER!"=="F" set "DRIVE_LOWER=f"
if /i "!DRIVE_LETTER!"=="G" set "DRIVE_LOWER=g"
if /i "!DRIVE_LETTER!"=="H" set "DRIVE_LOWER=h"
if /i "!DRIVE_LETTER!"=="I" set "DRIVE_LOWER=i"
if /i "!DRIVE_LETTER!"=="J" set "DRIVE_LOWER=j"
if /i "!DRIVE_LETTER!"=="K" set "DRIVE_LOWER=k"
if /i "!DRIVE_LETTER!"=="L" set "DRIVE_LOWER=l"
if /i "!DRIVE_LETTER!"=="M" set "DRIVE_LOWER=m"
if /i "!DRIVE_LETTER!"=="N" set "DRIVE_LOWER=n"
if /i "!DRIVE_LETTER!"=="O" set "DRIVE_LOWER=o"
if /i "!DRIVE_LETTER!"=="P" set "DRIVE_LOWER=p"
if /i "!DRIVE_LETTER!"=="Q" set "DRIVE_LOWER=q"
if /i "!DRIVE_LETTER!"=="R" set "DRIVE_LOWER=r"
if /i "!DRIVE_LETTER!"=="S" set "DRIVE_LOWER=s"
if /i "!DRIVE_LETTER!"=="T" set "DRIVE_LOWER=t"
if /i "!DRIVE_LETTER!"=="U" set "DRIVE_LOWER=u"
if /i "!DRIVE_LETTER!"=="V" set "DRIVE_LOWER=v"
if /i "!DRIVE_LETTER!"=="W" set "DRIVE_LOWER=w"
if /i "!DRIVE_LETTER!"=="X" set "DRIVE_LOWER=x"
if /i "!DRIVE_LETTER!"=="Y" set "DRIVE_LOWER=y"
if /i "!DRIVE_LETTER!"=="Z" set "DRIVE_LOWER=z"
REM Build POSIX path: /c/path/to/file (remove colon and prepend /)
set "POSIX_PATH=/!DRIVE_LOWER!!POSIX_PATH:~2!"
endlocal & set "%~2=%POSIX_PATH%"
goto :eof
