# SNMP support through Net-SNMP. vcpkg has no net-snmp port, so the library is
# built from source in a dedicated CI job (build_netsnmp.bat) and its install
# tree is pointed to here with SNMPINCDIR and SNMPLIBDIR. Leave both unset and
# the proxy builds without SNMP, exactly as before: the SNMP objects are always
# compiled, but with HAVE_NETSNMP undefined they collapse to reporting the
# checks unsupported.
#
# The library is the static netsnmp.lib configured with --linktype=static, so
# its net-snmp-config.h leaves NETSNMP_IMPORT empty and no DLL import applies.

!	IF "$(SNMPINCDIR)" != "" || "$(SNMPLIBDIR)" != ""

!		IF "$(SNMPINCDIR)" == ""
!			ERROR Please specify directory containing Net-SNMP header files (e. g. SNMPINCDIR="\somepath\include").
!		ENDIF

!		IF "$(SNMPLIBDIR)" == ""
!			ERROR Please specify directory containing Net-SNMP libraries (e. g. SNMPLIBDIR="\somepath\lib").
!		ENDIF

!		IF !EXISTS("$(SNMPINCDIR)\net-snmp\net-snmp-config.h")
!			ERROR SNMPINCDIR "$(SNMPINCDIR)" cannot find "$(SNMPINCDIR)\net-snmp\net-snmp-config.h"
!		ENDIF

!		IF !EXISTS("$(SNMPLIBDIR)\netsnmp.lib")
!			ERROR SNMPLIBDIR "$(SNMPLIBDIR)" cannot find "$(SNMPLIBDIR)\netsnmp.lib"
!		ENDIF

CFLAGS =	$(CFLAGS) /DHAVE_NETSNMP
INCS =	$(INCS) /I "$(SNMPINCDIR)"
LIBS =	$(LIBS) "$(SNMPLIBDIR)\netsnmp.lib"

!	ENDIF
