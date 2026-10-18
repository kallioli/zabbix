# libcurl. Optional for the agent, which only gains web.page.* over TLS from it, and required by the proxy
# (HTTP agent items, web scenarios, VMware), whose makefile insists on CURLINCDIR and CURLLIBDIR being set.
# The library is linked statically, which is why zlib has to follow it on the link line and why the IDN
# support needs normaliz.lib.
!	IF "$(CURL)" != ""
!		IF "$(CURLINCDIR)" == ""
CURLINCDIR=$(CURL)\include
!		ENDIF
!		IF "$(CURLLIBDIR)" == ""
CURLLIBDIR=$(CURL)\lib
!		ENDIF
!	ENDIF

!	IF "$(CURLINCDIR)" != ""
!		IF EXISTS("$(CURLINCDIR)\curl\curl.h")
CFLAGS =	$(CFLAGS) /DHAVE_LIBCURL /DCURL_STATICLIB
INCS =	$(INCS) /I "$(CURLINCDIR)"
!		ELSE
!			ERROR CURLINCDIR "$(CURLINCDIR)" cannot find "$(CURLINCDIR)\curl\curl.h"
!		ENDIF
!	ENDIF

!	IF "$(CURLLIBDIR)" != ""
!		IF EXISTS("$(CURLLIBDIR)\libcurl.lib")
LIBS = $(LIBS) "$(CURLLIBDIR)\libcurl.lib" Crypt32.lib
!		ELSEIF EXISTS("$(CURLLIBDIR)\libcurl-d.lib")
LIBS = $(LIBS) "$(CURLLIBDIR)\libcurl-d.lib" Crypt32.lib
!		ELSE
!			ERROR CURLLIBDIR "$(CURLLIBDIR)" cannot find neither "$(CURLLIBDIR)\libcurl.lib" nor "$(CURLLIBDIR)\libcurl-d.lib"
!		ENDIF

# Statically linked libcurl needs this for its IDN support.
LIBS = $(LIBS) normaliz.lib

!		IF "$(ZLIBLIBDIR)" != ""
# zlib 1.3.1 names its static library zs.lib on Windows, so that it does not
# collide with the import library of zlib1.dll. Older builds used zlib.lib or
# zlibstatic.lib.
!			IF EXISTS("$(ZLIBLIBDIR)\zs.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zs.lib"
!			ELSEIF EXISTS("$(ZLIBLIBDIR)\zlib.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlib.lib"
!			ELSEIF EXISTS("$(ZLIBLIBDIR)\zlibstatic.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlibstatic.lib"
!			ELSEIF EXISTS("$(ZLIBLIBDIR)\zsd.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zsd.lib"
!			ELSEIF EXISTS("$(ZLIBLIBDIR)\zlibd.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlibd.lib"
!			ELSE
!				ERROR ZLIBLIBDIR "$(ZLIBLIBDIR)" cannot find "$(ZLIBLIBDIR)\zs.lib", "$(ZLIBLIBDIR)\zlib.lib" nor "$(ZLIBLIBDIR)\zlibstatic.lib"
!			ENDIF
!		ENDIF

!		IF "$(ZLIBINCDIR)" != ""
!			IF EXISTS("$(ZLIBINCDIR)\zlib.h")
INCS =	$(INCS) /I "$(ZLIBINCDIR)"
!			ELSE
!				ERROR ZLIBINCDIR "$(ZLIBINCDIR)" cannot find "$(ZLIBINCDIR)\zlib.h"
!			ENDIF
!		ENDIF
!	ENDIF
