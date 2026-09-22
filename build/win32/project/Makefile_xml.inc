# libxml2 (from vcpkg). It enables VMware monitoring (with libcurl, already
# linked) and the XML XPath preprocessing step. Leave LIBXML2INCDIR and
# LIBXML2LIBDIR unset and the proxy builds without it, exactly as before: the
# VMware and XML objects are always compiled but collapse to reporting the
# feature unsupported when HAVE_LIBXML2 is undefined.
#
# vcpkg installs the headers under include\libxml2 (so <libxml/...> resolves
# from there), and the static library needs LIBXML_STATIC defined so its headers
# do not mark the symbols dllimport. libxml2's own zlib is the one already
# linked for libcurl.

!	IF "$(LIBXML2INCDIR)" != "" || "$(LIBXML2LIBDIR)" != ""

!		IF "$(LIBXML2INCDIR)" == ""
!			ERROR Please specify directory containing libxml2 header files (e. g. LIBXML2INCDIR="\somepath\include\libxml2").
!		ENDIF

!		IF "$(LIBXML2LIBDIR)" == ""
!			ERROR Please specify directory containing libxml2 libraries (e. g. LIBXML2LIBDIR="\somepath\lib").
!		ENDIF

!		IF !EXISTS("$(LIBXML2INCDIR)\libxml\parser.h")
!			ERROR LIBXML2INCDIR "$(LIBXML2INCDIR)" cannot find "$(LIBXML2INCDIR)\libxml\parser.h"
!		ENDIF

!		IF !EXISTS("$(LIBXML2LIBDIR)\libxml2.lib")
!			ERROR LIBXML2LIBDIR "$(LIBXML2LIBDIR)" cannot find "$(LIBXML2LIBDIR)\libxml2.lib"
!		ENDIF

CFLAGS =	$(CFLAGS) /DHAVE_LIBXML2 /DLIBXML_STATIC
INCS =	$(INCS) /I "$(LIBXML2INCDIR)"
LIBS =	$(LIBS) "$(LIBXML2LIBDIR)\libxml2.lib"

!	ENDIF
