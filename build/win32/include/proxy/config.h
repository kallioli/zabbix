/*
** Copyright (C) 2001-2026 Zabbix SIA
**
** This program is free software: you can redistribute it and/or modify it under the terms of
** the GNU Affero General Public License as published by the Free Software Foundation, version 3.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public License along with this program.
** If not, see <https://www.gnu.org/licenses/>.
**/

/* Build configuration for the Windows Zabbix proxy.                          */
/*                                                                            */
/* This mirrors build/win32/include/common/config.h, which serves the agent,  */
/* and adds what the proxy additionally requires: an embedded database, the   */
/* IPC service layer and the event loop it is built on.                       */
/*                                                                            */
/* It is selected ahead of the agent configuration through ADD_INCS in        */
/* Makefile_proxy, so both files can define config.h without colliding.       */

/* Define to os name for code managing */
#define ARCH "windows"

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if you have the <winsock2.h> header file. */
#define HAVE_WINSOCK2_H 1

/* Define to 1 if you have the <ws2tcpip.h> header file. */
#define HAVE_WS2TCPIP_H 1

/* Define to 1 if you have the <wspiapi.h> header file. */
#define HAVE_WSPIAPI_H 1

/* Define to 1 if you have the <conio.h> header file. */
#define HAVE_CONIO_H 1

/* Define to 1 if you have the <process.h> header file. */
#define HAVE_PROCESS_H 1

/* Define to 1 if you have the <pdh.h> header file. */
#define HAVE_PDH_H 1

/* Define to 1 if you have the <pdhmsg.h> header file. */
#define HAVE_PDHMSG_H 1

/* Define to 1 if you have the <psapi.h> header file. */
#define HAVE_PSAPI_H 1

/* Define to 1 if you have the <sys/timeb.h> header file. */
#define HAVE_SYS_TIMEB_H 1

/* Define to 1 if you have the <Winldap.h> header file. */
#define HAVE_WINLDAP_H 1

/* Define to 1 if you have the <Winber.h> header file. */
#define HAVE_WINBER_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <Iphlpapi.h> header file. */
#define HAVE_IPHLPAPI_H 1

/* Define to 1 if you have the <Netioapi.h> header file. */
#define HAVE_NETIOAPI_H 1

/* Define to 1 if 'sockaddr_storage.ss_family' exists. */
#define HAVE_SOCKADDR_STORAGE_SS_FAMILY 1

/* define to 1 if you have the <errno.h> header file */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <float.h> header file. Trend prediction needs
   DBL_MAX from it. */
#define HAVE_FLOAT_H 1

/* MSVC only declares M_PI and friends when this is set before <math.h>. */
#define _USE_MATH_DEFINES 1

/* MSVC supports variadic macros; without this the fallback definition of
   THIS_SHOULD_NEVER_HAPPEN_MSG expands into two statements and breaks the
   if/else around its callers. */
#define HAVE___VA_ARGS__ 1

/* --- proxy-specific --------------------------------------------------------*/

/* The proxy stores its buffer in an embedded SQLite database. MySQL and       */
/* PostgreSQL are not offered on Windows: the proxy is meant to run            */
/* self-contained on the host it monitors from.                                */
#define HAVE_SQLITE3 1
#define HAVE_FUNCTION_SQLITE3_OPEN_V2 1

/* The IPC service layer wires the proxy's worker threads together. On Windows */
/* it is backed by loopback sockets rather than AF_UNIX - see                  */
/* src/libs/zbxipcservice/ipcservice.c.                                        */
#define HAVE_IPCSERVICE 1

/* libevent drives both the IPC service and the asynchronous pollers. */
#define HAVE_LIBEVENT 1

/* Proxy-server traffic is compressed. */
#define HAVE_ZLIB 1

/* Required by HTTP agent items and web scenarios. */
#define HAVE_LIBCURL 1

/*
 * Deliberately NOT defined for this build:
 *
 *   HAVE_LIBXML2   - neutralises zbxvmware and XML preprocessing
 *   HAVE_NETSNMP   - no SNMP polling or trapping
 *   HAVE_ARES      - pollers fall back to synchronous name resolution
 *   HAVE_UNIXODBC  - no database monitor items
 *   HAVE_IPMI      - zbxipmi is excluded from the link entirely
 *   HAVE_SSH/SSH2, HAVE_LDAP
 *
 * The corresponding sources are still compiled and linked: each is guarded
 * internally and degrades to reporting the check as unsupported, which keeps
 * the object list identical to the POSIX build.
 */
