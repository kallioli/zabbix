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

rem --config and --linktype are both required by Configure; omitting linktype
rem makes it print usage and exit without generating a Makefile.
rem --config and --linktype are both required by Configure; omitting linktype
rem makes it print usage and exit without generating a Makefile.
echo === configuring (static, release, no ssl) ===
perl Configure --config=release --linktype=static --with-sdk --prefix="%PREFIX%" || exit /b 1

rem Net-SNMP typedefs mode_t (unsigned short) under a bare WIN32 guard, while
rem Zabbix typedefs it (int) behind _MODE_T_DEFINED. Whichever header lands
rem second then redefines the type - error C2371. Teach the generated header
rem the same _MODE_T_DEFINED guard so the first definition wins and the second
rem stands down, exactly as MSVC's own headers coordinate.
echo === guarding mode_t in the generated config header ===
powershell -NoProfile -Command "$f='net-snmp\net-snmp-config.h'; $c=Get-Content $f -Raw; $c=$c -replace '(?m)^\s*typedef\s+unsigned\s+short\s+mode_t;\s*$', \"#ifndef _MODE_T_DEFINED`r`n#define _MODE_T_DEFINED`r`ntypedef unsigned short mode_t;`r`n#endif\"; Set-Content $f $c -NoNewline" || exit /b 1

rem The proxy links the static C runtime (/MT) so the binary is self-contained.
rem Net-SNMP's Configure defaults its Makefiles to the dynamic runtime (/MD),
rem whose objects import the CRT from a DLL (__imp_ symbols); linking those into
rem a /MT binary leaves setlocale, mktemp, putenv and friends unresolved. Force
rem every generated Makefile to the static runtime so both agree.
echo === forcing the static CRT (/MT) in the generated makefiles ===
powershell -NoProfile -Command "Get-ChildItem -Path . -Recurse -Filter Makefile | ForEach-Object { $p=$_.FullName; $c=Get-Content $p -Raw; if ($c -match '/MDd|/MD') { $c=$c -replace '/MDd','/MTd' -replace '/MD','/MT'; Set-Content $p $c -NoNewline; Write-Host ('patched ' + $p) } }" || exit /b 1

echo === building ===
nmake || exit /b 1

echo === installing to %OUTDIR% ===
nmake install || exit /b 1

rem nmake install lays down the .exe tools, MIBs and a couple of headers, but
rem not the API header tree and not the static library the proxy links. Both
rem have to be gathered by hand.
rem
rem Headers: the checked-in API headers live in the source include/net-snmp
rem tree; Configure generated net-snmp-config.h (and a few others) under
rem win32/net-snmp, which overlay the source copies.
echo === assembling the header tree ===
xcopy /e /i /y "..\include\net-snmp" "%OUTDIR%\include\net-snmp" >nul || exit /b 1
xcopy /e /i /y "net-snmp" "%OUTDIR%\include\net-snmp" >nul || exit /b 1
if not exist "%OUTDIR%\include\net-snmp\net-snmp-includes.h" (
    echo ERROR: net-snmp-includes.h did not land in the include tree
    exit /b 1
)

rem Library: the static netsnmp.lib is left in a component release directory,
rem not installed. Copy every .lib produced into the prefix and list them.
echo === all .lib produced under win32 ===
dir /s /b *.lib
if not exist "%OUTDIR%\lib" mkdir "%OUTDIR%\lib"
for /r %%f in (*.lib) do copy /y "%%f" "%OUTDIR%\lib\" >nul
if not exist "%OUTDIR%\lib\netsnmp.lib" (
    echo ERROR: netsnmp.lib did not land in the lib directory
    exit /b 1
)

echo === installed libraries ===
dir /b "%OUTDIR%\lib"

endlocal
