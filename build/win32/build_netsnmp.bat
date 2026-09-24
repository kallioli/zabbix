@echo off
rem Build Net-SNMP (static, MSVC x64) for the Windows proxy: the netsnmp.lib the
rem proxy's SNMP checks link, and snmptrapd.exe, which the package ships as the
rem trap receiver.
rem
rem vcpkg has no net-snmp port, so the library the proxy's SNMP checks need is
rem built here from source with Net-SNMP's own Perl/nmake win32 system. Run from
rem a shell where vcvars64 has already set up the MSVC environment; Strawberry
rem Perl (on the GitHub runners) provides the perl that Configure is written in.
rem
rem   build_netsnmp.bat <version> <install-dir> <vcpkg-dir>
rem
rem Configure runs --with-ssl against the vcpkg OpenSSL, so SNMPv3 gets AES and
rem SHA-2 rather than only the built-in MD5/SHA1/DES. The install tree left under
rem <install-dir> holds include\, lib\netsnmp.lib and bin\snmptrapd.exe.

setlocal enabledelayedexpansion

set "VER=%~1"
set "OUTDIR=%~2"
set "VCPKG=%~3"

if "%VER%"=="" ( echo usage: build_netsnmp.bat ^<version^> ^<install-dir^> ^<vcpkg-dir^> & exit /b 2 )
if "%OUTDIR%"=="" ( echo usage: build_netsnmp.bat ^<version^> ^<install-dir^> ^<vcpkg-dir^> & exit /b 2 )
if "%VCPKG%"=="" ( echo usage: build_netsnmp.bat ^<version^> ^<install-dir^> ^<vcpkg-dir^> & exit /b 2 )

rem Net-SNMP wants forward-slash paths (README.win32).
set "PREFIX=%OUTDIR:\=/%"
set "SSLINC=%VCPKG:\=/%/include"
set "SSLLIB=%VCPKG:\=/%/lib"

if not exist "%VCPKG%\include\openssl\ssl.h" (
    echo ERROR: OpenSSL headers not found under "%VCPKG%\include\openssl"
    exit /b 1
)

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
rem makes it print usage and exit without generating a Makefile. --with-ssl
rem turns on the OpenSSL-backed SNMPv3 crypto (AES, SHA-2); the include and lib
rem dirs point at the vcpkg OpenSSL the proxy also links.
echo === configuring (static, release, ssl) ===
perl Configure --config=release --linktype=static --with-sdk ^
    --with-ssl --with-sslincdir="%SSLINC%" --with-ssllibdir="%SSLLIB%" ^
    --prefix="%PREFIX%" || exit /b 1

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

rem Net-SNMP asks the linker for OpenSSL as libcrypto64MT.lib / libssl64MT.lib
rem (its own naming for a Win64 static-MT OpenSSL), through auto-link pragmas its
rem objects carry. vcpkg names them libcrypto.lib / libssl.lib, and that pragma
rem would go looking for the 64MT names at the proxy link too. Rewrite the names
rem to vcpkg's in the generated headers and makefiles, so net-snmp and the proxy
rem resolve OpenSSL from the one set of libraries.
rem Patch the whole tree, not just win32: the pragma may sit in a source header
rem under ..\include that both the tools and the proxy compile against.
echo === matching the OpenSSL library names to vcpkg ===
powershell -NoProfile -Command "Get-ChildItem -Path .. -Recurse -Include *.h,Makefile | ForEach-Object { $p=$_.FullName; $c=Get-Content $p -Raw; if ($c -match 'lib(crypto|ssl)64MT') { $c=$c -replace 'libcrypto64MT','libcrypto' -replace 'libssl64MT','libssl'; Set-Content $p $c -NoNewline; Write-Host ('patched ' + $p) } }" || exit /b 1

rem The proxy links only netsnmp.lib, but the Windows package also ships
rem snmptrapd.exe as the trap receiver (Zabbix's SNMP trapper is a file tailer;
rem something has to write the traps into that file, and there is no perl on
rem Windows for the usual receiver script). snmptrapd links netsnmp.lib plus the
rem agent, MIB-module and trap-daemon libraries, so those four are built first.
rem Build each subdirectory the same way the proven libsnmp build does
rem (cd <dir> & nmake), in the dependency order the top-level Makefile's "libs"
rem target uses, rather than relying on the aggregate target and top Makefile.
echo === building the static libraries ===
for %%d in (libagent libsnmp libnetsnmptrapd netsnmpmibs) do (
    echo --- %%d ---
    cd %%d || exit /b 1
    nmake || exit /b 1
    cd .. || exit /b 1
)

rem net-snmp's stock snmptrapd link line lists only advapi32/ws2_32/kernel32/
rem user32 - fine for its own internal-crypto build, but this tree is
rem --with-ssl against vcpkg's static OpenSSL, which resolves through crypt32 and
rem bcrypt (exactly what the proxy's own OpenSSL link adds). The OpenSSL import
rem libraries are named and their directory put on the link path here too, rather
rem than trusting the auto-link pragma alone. Inject all of it into the generated
rem snmptrapd Makefile before building the daemon.
echo === adding OpenSSL/crypt32/bcrypt to the snmptrapd link line ===
powershell -NoProfile -Command "$f='snmptrapd\Makefile'; $c=Get-Content $f -Raw; $add='netsnmptrapd.lib crypt32.lib bcrypt.lib \"%VCPKG%\lib\libcrypto.lib\" \"%VCPKG%\lib\libssl.lib\" /libpath:\"%VCPKG%\lib\" advapi32.lib'; $n=$c -replace 'netsnmptrapd\.lib advapi32\.lib', $add; if ($n -eq $c) { Write-Error 'snmptrapd link-line anchor not found; net-snmp Makefile layout changed'; exit 1 }; Set-Content $f $n -NoNewline" || exit /b 1

echo === building snmptrapd.exe ===
cd snmptrapd || exit /b 1
nmake || exit /b 1
cd .. || exit /b 1

rem The API header tree is not assembled by a plain library build. The
rem checked-in headers live in the source include/net-snmp tree; Configure
rem generated net-snmp-config.h (and a few others) under win32/net-snmp, which
rem overlay the source copies.
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

rem The trap receiver. Configure with --config=release leaves it in win32\bin\
rem release; hand it to the install tree so the MSI can pick it up beside the
rem proxy. Its winservice.obj lets it register as a Windows service on its own.
echo === collecting snmptrapd.exe ===
if not exist "%OUTDIR%\bin" mkdir "%OUTDIR%\bin"
for /r %%f in (snmptrapd.exe) do copy /y "%%f" "%OUTDIR%\bin\" >nul
if not exist "%OUTDIR%\bin\snmptrapd.exe" (
    echo ERROR: snmptrapd.exe was not built
    exit /b 1
)
dir /b "%OUTDIR%\bin"

endlocal
