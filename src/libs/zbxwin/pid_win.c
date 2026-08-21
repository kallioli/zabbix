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

/* Windows counterpart of src/libs/zbxnix/coredump.c and the backtrace helper.
 *
 * Crash reporting is already handled for Windows by src/libs/zbxwin32/fatal.c,
 * which writes a minidump through an unhandled exception filter. There is no
 * core dump resource limit to lower, and no equivalent of walking the stack
 * from arbitrary code the way zbx_backtrace() does on Unix.
 */

#include "zbxcommon.h"

#include "zbxnix.h"
#include "zbxlog.h"

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
/******************************************************************************
 *                                                                            *
 * Purpose: prevents a core dump from leaking private keys                    *
 *                                                                            *
 * Comments: on Unix this lowers RLIMIT_CORE, so that a crash after the TLS   *
 *           material is loaded cannot write it to disk. Windows writes a     *
 *           minidump, which does not carry the full address space, so there  *
 *           is nothing to disable.                                           *
 *                                                                            *
 ******************************************************************************/
int	zbx_coredump_disable(void)
{
	return SUCCEED;
}
#endif

void	zbx_backtrace(void)
{
	zabbix_log(LOG_LEVEL_DEBUG, "backtrace is not available on Windows;"
			" a minidump is written on an unhandled exception");
}
