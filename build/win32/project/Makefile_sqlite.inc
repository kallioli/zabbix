!	IF "$(SQLITE3INCDIR)" != ""
!		IF EXISTS("$(SQLITE3INCDIR)\sqlite3.h")
INCS =	$(INCS) /I "$(SQLITE3INCDIR)"
!		ELSE
!			ERROR SQLITE3INCDIR "$(SQLITE3INCDIR)" cannot find "$(SQLITE3INCDIR)\sqlite3.h"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing SQLite3 header files (e. g. SQLITE3INCDIR="\somepath\include").
!	ENDIF

!	IF "$(SQLITE3LIBDIR)" != ""
!		IF EXISTS("$(SQLITE3LIBDIR)\sqlite3.lib")
LIBS = $(LIBS) "$(SQLITE3LIBDIR)\sqlite3.lib"
!		ELSEIF EXISTS("$(SQLITE3LIBDIR)\sqlite3d.lib")
LIBS = $(LIBS) "$(SQLITE3LIBDIR)\sqlite3d.lib"
!		ELSE
!			ERROR SQLITE3LIBDIR "$(SQLITE3LIBDIR)" cannot find neither "$(SQLITE3LIBDIR)\sqlite3.lib" nor "$(SQLITE3LIBDIR)\sqlite3d.lib"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing SQLite3 libraries (e. g. SQLITE3LIBDIR="\somepath\lib").
!	ENDIF
