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

/* Service start-up and worker lifecycle for the Windows build.
 *
 * Replaces the fork-and-daemonise flow of zbxnix/daemon.c: the workers are
 * started as threads and the main thread hands control to the Windows service
 * dispatcher, or stays in the console when started with --foreground.
 *
 * Implements: zbx_daemon_start, zbx_daemon_stop, zbx_set_child_pids,
 * zbx_set_on_exit_args, ZBX_IS_RUNNING, ZBX_EXIT_STATUS,
 * zbx_set_exiting_with_fail, zbx_set_exiting_with_succeed.
 */

/* placeholder until the platform layer lands - keeps the translation unit
   from being empty, which MSVC reports as LNK4221 */
const int	zbx_win_daemon_win_placeholder = 0;
