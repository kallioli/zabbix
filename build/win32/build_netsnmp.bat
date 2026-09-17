@echo off
rem Build Net-SNMP as a static library for the MSVC x64 proxy.
rem
rem vcpkg has no net-snmp port, so the library the proxy's SNMP checks need is
rem built here from source with Net-SNMP's own Perl/nmake win32 system. Run from
rem a shell where vcvars64 has already set up the MSVC environment; Strawberry
rem Perl (on the GitHub runners) provides the perl that Configure is written in.
rem
rem   build_netsnmp.bat <version> <install-dir>
rem
rem Milestone 1 is SNMPv1/v2c: Configure runs without --with-ssl, so no OpenSSL
rem wiring is in play. USM symbols are still compiled, so the proxy links; only
rem the SNMPv3 privacy and authentication protocols are absent until a later
rem pass adds --with-ssl against the OpenSSL that vcpkg already provides.

setlocal enabledelayedexpansion

set "VER=%~1"
set "OUTDIR=%~2"

if "%VER%"=="" ( echo usage: build_netsnmp.bat ^<version^> ^<install-dir^> & exit /b 2 )
if "%OUTDIR%"=="" ( echo usage: build_netsnmp.bat ^<version^> ^<install-dir^> & exit /b 2 )

rem Net-SNMP wants a forward-slash prefix (README.win32).
set "PREFIX=%OUTDIR:\=/%"

rem The official release archives live on SourceForge; there is no GitHub
rem release asset. curl -L follows the mirror redirects.
echo === downloading net-snmp %VER% ===
curl -L --fail -o netsnmp.zip ^
    https://downloads.sourceforge.net/project/net-snmp/net-snmp/%VER%/net-snmp-%VER%.zip || exit /b 1

echo === extracting ===
tar -xf netsnmp.zip || exit /b 1

rem A sick SourceForge mirror can serve an HTML page with a 200 that curl does
rem not catch; if so the source tree is not there and we stop before nmake.
if not exist net-snmp-%VER%\win32\Configure (
    echo ERROR: net-snmp-%VER%\win32\Configure missing - download was not the source archive
    exit /b 1
)

cd net-snmp-%VER%\win32 || exit /b 1

echo === configuring (static, release, no ssl) ===
perl Configure --config=release --with-sdk --prefix="%PREFIX%" || exit /b 1

echo === building ===
nmake || exit /b 1

echo === installing to %OUTDIR% ===
nmake install || exit /b 1

echo === installed tree ===
dir /s /b "%OUTDIR%"

endlocal
