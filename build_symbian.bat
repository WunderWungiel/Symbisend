@echo off
:: Builds Symbisend for Symbian^3/Anna/Belle (Qt 4.7.4 SDK, GCCE) and produces a self-signed .sis.
:: In-source build: Qt 4.7 + sbsv2 does not support shadow builds.
:: Usage: build_symbian.bat [release|debug]   (default: release)
setlocal
set PROJDIR=%~dp0
set CONF=%1
if "%CONF%"=="" set CONF=release

call C:\QtSDK\Symbian\SDKs\Symbian3Qt474\env.bat
pushd "%PROJDIR%"

qmake.exe Symbisend.pro -r -spec symbian-sbsv2 "CONFIG+=%CONF%" || goto :fail
make %CONF%-gcce -w || goto :fail
:: -d: keep the developer-certificate capabilities (ReadDeviceData, ... for the
:: gallery picker) instead of stripping them; the self-signed .sis then needs a
:: phone that accepts unsigned packages.
make sis QT_SIS_TARGET=%CONF%-gcce QT_SIS_OPTIONS=-d || goto :fail

echo.
echo === Build OK ===
dir /b *.sis
popd
exit /b 0

:fail
echo.
echo === Build FAILED ===
popd
exit /b 1
