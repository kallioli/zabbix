# ODBC database monitoring (the "Database monitor" item type).
#
# Unlike the other libraries the proxy needs, ODBC is not a third party on
# Windows: the platform ships the ODBC Driver Manager (odbc32) in the SDK, so
# there is nothing to fetch or point at. The monitoring code is the same ISO
# CLI as on Unix, gated by HAVE_UNIXODBC; odbc32.lib is always on the default
# library path. The per-database drivers (SQL Server, PostgreSQL, MySQL, ...)
# are installed by the operator, exactly as unixODBC drivers are on Unix.
CFLAGS =	$(CFLAGS) /DHAVE_UNIXODBC
LIBS =	$(LIBS) odbc32.lib
