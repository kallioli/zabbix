!	IF "$(CURLINCDIR)" != ""
!		IF EXISTS("$(CURLINCDIR)\curl\curl.h")
CFLAGS =	$(CFLAGS) /DCURL_STATICLIB
INCS =	$(INCS) /I "$(CURLINCDIR)"
!		ELSE
!			ERROR CURLINCDIR "$(CURLINCDIR)" cannot find "$(CURLINCDIR)\curl\curl.h"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing libcurl header files (e. g. CURLINCDIR="\somepath\include").
!	ENDIF

!	IF "$(CURLLIBDIR)" != ""
!		IF EXISTS("$(CURLLIBDIR)\libcurl.lib")
LIBS = $(LIBS) "$(CURLLIBDIR)\libcurl.lib"
!		ELSEIF EXISTS("$(CURLLIBDIR)\libcurl-d.lib")
LIBS = $(LIBS) "$(CURLLIBDIR)\libcurl-d.lib"
!		ELSE
!			ERROR CURLLIBDIR "$(CURLLIBDIR)" cannot find neither "$(CURLLIBDIR)\libcurl.lib" nor "$(CURLLIBDIR)\libcurl-d.lib"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing libcurl libraries (e. g. CURLLIBDIR="\somepath\lib").
!	ENDIF

# Statically linked libcurl pulls these in for SChannel/Winsock/LDAP support.
LIBS = $(LIBS) crypt32.lib normaliz.lib

!	IF "$(ZLIBLIBDIR)" != ""
!		IF EXISTS("$(ZLIBLIBDIR)\zlib.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlib.lib"
!		ELSEIF EXISTS("$(ZLIBLIBDIR)\zlibstatic.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlibstatic.lib"
!		ELSEIF EXISTS("$(ZLIBLIBDIR)\zlibd.lib")
LIBS = $(LIBS) "$(ZLIBLIBDIR)\zlibd.lib"
!		ELSE
!			ERROR ZLIBLIBDIR "$(ZLIBLIBDIR)" cannot find "$(ZLIBLIBDIR)\zlib.lib", "$(ZLIBLIBDIR)\zlibstatic.lib" nor "$(ZLIBLIBDIR)\zlibd.lib"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing zlib libraries (e. g. ZLIBLIBDIR="\somepath\lib").
!	ENDIF

!	IF "$(ZLIBINCDIR)" != ""
!		IF EXISTS("$(ZLIBINCDIR)\zlib.h")
INCS =	$(INCS) /I "$(ZLIBINCDIR)"
!		ELSE
!			ERROR ZLIBINCDIR "$(ZLIBINCDIR)" cannot find "$(ZLIBINCDIR)\zlib.h"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing zlib header files (e. g. ZLIBINCDIR="\somepath\include").
!	ENDIF
