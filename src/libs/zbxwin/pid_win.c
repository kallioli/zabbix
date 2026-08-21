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

#include "zbxcommon.h"

/* Single-instance guard for the Windows build.
 *
 * A PID file has no meaning for a Windows service; a named mutex gives the
 * same "only one proxy per configuration" guarantee.
 *
 * Implements: zbx_create_pid_file, zbx_drop_pid_file, zbx_coredump_disable,
 * zbx_backtrace.
 */

/* placeholder until the platform layer lands - keeps the translation unit
   from being empty, which MSVC reports as LNK4221 */
const int	zbx_win_pid_win_placeholder = 0;
