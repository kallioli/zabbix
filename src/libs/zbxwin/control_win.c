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

/* Runtime control for the Windows build.
 *
 * zbxnix/control.c reaches other workers by signalling their PIDs. Here every
 * worker lives in the same process, so runtime control messages travel over
 * the IPC service directly.
 *
 * Implements: zbx_signal_process_by_type, zbx_signal_process_by_pid,
 * zbx_sigusr_send, zbx_set_sigusr_handler, zbx_parse_rtc_options.
 */

/* placeholder until the platform layer lands - keeps the translation unit
   from being empty, which MSVC reports as LNK4221 */
const int	zbx_win_control_win_placeholder = 0;
