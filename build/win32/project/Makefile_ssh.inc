# SSH checks through libssh2 (from vcpkg). Leave SSHINCDIR and SSHLIBDIR unset
# and the proxy builds without SSH, exactly as before: ssh2_run.c is guarded by
# HAVE_SSH2 and collapses to an empty object, and checks_ssh.c reports the SSH
# item type unsupported. libssh2's own crypto and zlib come from the OpenSSL and
# zlib already linked for TLS and libcurl.

!	IF "$(SSHINCDIR)" != "" || "$(SSHLIBDIR)" != ""

!		IF "$(SSHINCDIR)" == ""
!			ERROR Please specify directory containing libssh2 header files (e. g. SSHINCDIR="\somepath\include").
!		ENDIF

!		IF "$(SSHLIBDIR)" == ""
!			ERROR Please specify directory containing libssh2 libraries (e. g. SSHLIBDIR="\somepath\lib").
!		ENDIF

!		IF !EXISTS("$(SSHINCDIR)\libssh2.h")
!			ERROR SSHINCDIR "$(SSHINCDIR)" cannot find "$(SSHINCDIR)\libssh2.h"
!		ENDIF

!		IF !EXISTS("$(SSHLIBDIR)\libssh2.lib")
!			ERROR SSHLIBDIR "$(SSHLIBDIR)" cannot find "$(SSHLIBDIR)\libssh2.lib"
!		ENDIF

CFLAGS =	$(CFLAGS) /DHAVE_SSH2
INCS =	$(INCS) /I "$(SSHINCDIR)"
LIBS =	$(LIBS) "$(SSHLIBDIR)\libssh2.lib"

!	ENDIF
