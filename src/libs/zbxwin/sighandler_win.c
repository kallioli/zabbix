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

/* Termination handling for the Windows build.
 *
 * There are no POSIX signals to catch: console events and service control
 * requests drive the same exit flags that zbxnix/sighandler.c sets. The signal
 * mask operations have no counterpart and collapse to no-ops.
 *
 * Implements: zbx_set_common_signal_handlers, zbx_set_child_signal_handler,
 * zbx_unset_child_signal_handler, zbx_set_metric_thread_signal_handler,
 * zbx_block_signals, zbx_block_thread_signals, zbx_unblock_signals,
 * zbx_init_thread_signal_handler, zbx_set_exit_on_terminate,
 * zbx_unset_exit_on_terminate, zbx_log_exit_signal, zbx_init_library_nix.
 */

/* placeholder until the platform layer lands - keeps the translation unit
   from being empty, which MSVC reports as LNK4221 */
const int	zbx_win_sighandler_win_placeholder = 0;
