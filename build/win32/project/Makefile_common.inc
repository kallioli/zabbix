CC = cl.exe
LINKER = link.exe
MC = mc.exe
MT = mt.exe
RC = rc.exe

MESSAGES_H = messages.h
RESOURCE_H = resource.h
RESOURCE_RC = resource.rc
DESC_H = $(PROJECTNAME)_desc.h
RESOURCE_RES = $(PROJECTNAME).res

INCS = $(ADD_INCS) /I $(PROJECTDIR) /I .\ /I ..\include  /I ..\include\common /I ..\..\..\include /I ..\..\..\include\common

# a target may raise this; the proxy needs Vista for condition variables
!IF "$(WIN32_WINNT)" == ""
WIN32_WINNT = 0x0502
!ENDIF

# include/version.h carries release-tooling placeholders for these when the tree
# comes from git rather than from a release tarball, and the version resource
# does not compile without them. Override on the command line to stamp a build.
!IF "$(ZBX_REVISION)" == ""
ZBX_REVISION = 0
!ENDIF
!IF "$(ZBX_RC_NUM)" == ""
ZBX_RC_NUM = 0
!ENDIF

!IF "$(CPU)" == "i386" || "$(CPU)" == "x86"
COMMON_FLAGS = /D _WIN32_WINNT=$(WIN32_WINNT)
ADD_LFLAGS = $(ADD_LFLAGS) /SUBSYSTEM:"CONSOLE,5.01"
!ELSEIF "$(CPU)" == "AMD64"
COMMON_FLAGS = /D _WIN32_WINNT=$(WIN32_WINNT)
ADD_LFLAGS = $(ADD_LFLAGS) /SUBSYSTEM:"CONSOLE,5.02"
!ENDIF

!IF "$(DEBUG)" != ""
OPTS = /Ob0
!ELSE
OPTS = /O2
!ENDIF

COMMON_FLAGS = $(COMMON_FLAGS) /nologo $(OPTS) /GF /FD /EHsc /MT /Gy /W3 /c /D _WINDOWS /D _CONSOLE /D UNICODE \
	/D _UNICODE /D HAVE_WINLDAP_H /D HAVE_ASSERT_H /D ZABBIX_SERVICE /D "_VC80_UPGRADE=0x0600" /D HAVE_IPV6 \
	/D ZABBIX_VERSION_REVISION=$(ZBX_REVISION) /D ZABBIX_VERSION_RC_NUM=$(ZBX_RC_NUM) \
	$(EXTRA_CFLAGS)

CFLAGS = $(CFLAGS) $(INCS) $(ADD_CFLAGS) $(COMMON_FLAGS) /TC

CPPFLAGS = $(INCS) $(ADD_CFLAGS) $(COMMON_FLAGS) /TP

LFLAGS = /NOLOGO $(ADD_LFLAGS) /INCREMENTAL:NO /MANIFEST /MANIFESTFILE:"$(TARGETNAME).$(TARGETEXT).manifest" \
	/MANIFESTUAC:"level='asInvoker' uiAccess='false'" /DYNAMICBASE:NO /PDB:$(TARGETDIR)\$(TARGETNAME).pdb

RFLAGS = $(RFLAGS) $(ADD_RFLAGS) /d _WINDOWS \
	/d ZABBIX_VERSION_REVISION=$(ZBX_REVISION) /d ZABBIX_VERSION_RC_NUM=$(ZBX_RC_NUM)
