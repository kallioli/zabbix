# LDAP simple check (net.tcp.service[ldap]) and LDAP network discovery.
#
# Like ODBC, LDAP is not a third party on Windows: the SDK ships WinLDAP
# (wldap32), which exposes the same RFC 1823 C API the checks use, gated by
# HAVE_LDAP. On Windows the code includes <winldap.h> / <winber.h> instead of
# <ldap.h>. wldap32.lib is already in the base link, so only the flag is set
# here.
CFLAGS =	$(CFLAGS) /DHAVE_LDAP
