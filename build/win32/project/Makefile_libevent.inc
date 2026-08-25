!	IF "$(LIBEVENTINCDIR)" != ""
!		IF EXISTS("$(LIBEVENTINCDIR)\event2\event.h")
INCS =	$(INCS) /I "$(LIBEVENTINCDIR)"
!		ELSE
!			ERROR LIBEVENTINCDIR "$(LIBEVENTINCDIR)" cannot find "$(LIBEVENTINCDIR)\event2\event.h"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing libevent header files (e. g. LIBEVENTINCDIR="\somepath\include").
!	ENDIF

!	IF "$(LIBEVENTLIBDIR)" != ""
!		IF EXISTS("$(LIBEVENTLIBDIR)\event_core.lib")
LIBS = $(LIBS) "$(LIBEVENTLIBDIR)\event_core.lib" "$(LIBEVENTLIBDIR)\event_extra.lib"
!		ELSEIF EXISTS("$(LIBEVENTLIBDIR)\event.lib")
LIBS = $(LIBS) "$(LIBEVENTLIBDIR)\event.lib"
!		ELSE
!			ERROR LIBEVENTLIBDIR "$(LIBEVENTLIBDIR)" cannot find neither "$(LIBEVENTLIBDIR)\event_core.lib" nor "$(LIBEVENTLIBDIR)\event.lib"
!		ENDIF
!	ELSE
!		ERROR Please specify directory containing libevent libraries (e. g. LIBEVENTLIBDIR="\somepath\lib").
!	ENDIF

# libevent on Windows resolves its event backend through Winsock and Iphlpapi,
# both of which the proxy already links.
