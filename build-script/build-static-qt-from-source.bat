@echo on
REM To install OpenTerface QT with static OpenSSL support, you can run this script as an administrator.

setlocal enabledelayedexpansion

REM Configuration
set QT_VERSION=6.5.3
set QT_MAJOR_VERSION=6.5
set INSTALL_PREFIX=C:\Qt6
set BUILD_DIR=%cd%\qt-build
set MODULES=qtbase qtshadertools qtmultimedia qtsvg qtserialport qttools
set DOWNLOAD_BASE_URL=https://download.qt.io/archive/qt/%QT_MAJOR_VERSION%/%QT_VERSION%/submodules
set VCPKG_DIR=C:\vcpkg
set OPENSSL_DIR=%VCPKG_DIR%\installed\x64-mingw-static
set OPENSSL_LIB_DIR=%OPENSSL_DIR%\lib
set OPENSSL_INCLUDE_DIR=%OPENSSL_DIR%\include

set PATH=C:\ProgramData\chocolatey\bin;C:\ProgramData\chocolatey\lib\ninja\tools;C:\mingw64\bin;%PATH%

REM Check for Ninja
where ninja >nul 2>nul
if %errorlevel% neq 0 (
    echo Ninja is not installed. Please install Ninja and ensure it is in your PATH.
    exit /b 1
)

REM Check for OpenSSL static libraries
echo "Checking OpenSSL static libraries for Windows 11 compatibility..."
dir "%OPENSSL_LIB_DIR%"
if not exist "%OPENSSL_LIB_DIR%\libcrypto.a" (
    echo OpenSSL static library libcrypto.a not found in %OPENSSL_LIB_DIR%. Please install OpenSSL static libraries.
    echo For Windows 11, ensure you're using the latest vcpkg version:
    echo   vcpkg install openssl:x64-mingw-static --triplet x64-mingw-static
    exit /b 1
)
if not exist "%OPENSSL_LIB_DIR%\libssl.a" (
    echo OpenSSL static library libssl.a not found in %OPENSSL_LIB_DIR%. Please install OpenSSL static libraries.
    echo For Windows 11, ensure you're using the latest vcpkg version:
    echo   vcpkg install openssl:x64-mingw-static --triplet x64-mingw-static
    exit /b 1
)

REM Verify OpenSSL version for Windows 11 compatibility
echo "Verifying OpenSSL version for Windows 11 compatibility..."
if exist "%OPENSSL_INCLUDE_DIR%\openssl\opensslv.h" (
    findstr "OPENSSL_VERSION_TEXT" "%OPENSSL_INCLUDE_DIR%\openssl\opensslv.h"
) else (
    echo Warning: Could not verify OpenSSL version
)

REM Create build directory
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Download and extract modules
for %%m in (%MODULES%) do (
    if not exist "%%m" (
        curl -L -o "%%m.zip" "%DOWNLOAD_BASE_URL%/%%m-everywhere-src-%QT_VERSION%.zip"
        powershell -command "Expand-Archive -Path %%m.zip -DestinationPath ."
        move "%%m-everywhere-src-%QT_VERSION%" "%%m"
        del "%%m.zip"
    )
)

REM Build qtbase first
cd "%BUILD_DIR%\qtbase"
mkdir build
cd build
cmake -G "Ninja" ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DFEATURE_dbus=ON ^
    -DFEATURE_sql=OFF ^
    -DFEATURE_testlib=OFF ^
    -DFEATURE_icu=OFF ^
    -DFEATURE_opengl=ON ^
    -DFEATURE_openssl=ON ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_DIR%" ^
    -DOPENSSL_INCLUDE_DIR="%OPENSSL_INCLUDE_DIR%" ^
    -DOPENSSL_CRYPTO_LIBRARY="%OPENSSL_LIB_DIR%\libcrypto.a" ^
    -DOPENSSL_SSL_LIBRARY="%OPENSSL_LIB_DIR%\libssl.a" ^
    -DCMAKE_C_FLAGS="-I%OPENSSL_INCLUDE_DIR% -DOPENSSL_STATIC_LINK" ^
    -DCMAKE_CXX_FLAGS="-I%OPENSSL_INCLUDE_DIR% -DOPENSSL_STATIC_LINK" ^
    -DCMAKE_EXE_LINKER_FLAGS="-L%OPENSSL_LIB_DIR% -lssl -lcrypto -lws2_32 -lcrypt32 -ladvapi32 -luser32 -lgdi32" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static ^
    ..
ninja
ninja install

REM Build other modules (including qttools)
for %%m in (%MODULES%) do (
    if /I not "%%m"=="qtbase" (
        cd "%BUILD_DIR%\%%m"
        mkdir build
        cd build
        cmake -G "Ninja" ^
            -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
            -DCMAKE_PREFIX_PATH="%INSTALL_PREFIX%" ^
            -DBUILD_SHARED_LIBS=OFF ^
            -DOPENSSL_DIR="%OPENSSL_DIR%" ^
            -DOPENSSL_INCLUDE_DIR="%OPENSSL_INCLUDE_DIR%" ^
            -DOPENSSL_LIB_DIR="%OPENSSL_LIB_DIR%" ^
            -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" ^
            ..
        ninja
        ninja install
    )
)

REM Quick fix: Add -loleaut32 to qnetworklistmanager.prl
set PRL_FILE=%INSTALL_PREFIX%\plugins\networkinformation\qnetworklistmanager.prl
if exist "%PRL_FILE%" (
    echo Updating %PRL_FILE% to include -loleaut32...
    echo QMAKE_PRL_LIBS += -loleaut32 >> "%PRL_FILE%"
) else (
    echo Warning: %PRL_FILE% not found. Please check the build process.
)

REM Verify lupdate
if exist "%INSTALL_PREFIX%\bin\lupdate.exe" (
    echo lupdate.exe successfully built!
) else (
    echo Error: lupdate.exe not found in %INSTALL_PREFIX%\bin
    exit /b 1
)