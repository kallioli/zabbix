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

/* Windows counterpart of src/libs/zbxnix/daemon.c. There is nothing to daemonise: zbx_service_start() either runs */
/* MAIN_ZABBIX_ENTRY() in the console when started with --foreground, or hands the process to the service control */
/* dispatcher, which calls it from the service thread. Dropping privileges has no counterpart either - a Windows */
/* service runs as the account it was installed with. What remains is registering the callbacks that the Unix */
/* implementation stores at the same point. */

#include "zbxcommon.h"

#include "zbxnix.h"
#include "win_internal.h"
#include "zbxwinservice.h"
#include "zbxthreads.h"

static zbx_get_config_str_f	get_pid_file_pathname_cb = NULL;
static zbx_get_threads_f	get_threads_func_cb = NULL;
static zbx_get_config_int_f	get_threads_num_func_cb = NULL;

int	zbx_daemon_start(int allow_root, const char *user, unsigned int flags,
		zbx_get_config_str_f get_pid_file_cb, zbx_on_exit_t zbx_on_exit_cb_arg, int config_log_type,
		const char *config_log_file, zbx_signal_redirect_f signal_redirect_cb,
		zbx_get_threads_f get_threads_cb, zbx_get_config_int_f get_threads_num_cb)
{
	/* a Windows service runs as its configured account, so there is no user to switch to and no root to refuse */
	ZBX_UNUSED(allow_root);
	ZBX_UNUSED(user);

	/* stdio is not redirected: the service writes to its log file, and in the foreground the console is where */
	/* the operator wants the output */
	ZBX_UNUSED(config_log_type);
	ZBX_UNUSED(config_log_file);

	/* runtime control is delivered over IPC rather than signals, so there is no handler to redirect - see */
	/* control_win.c */
	ZBX_UNUSED(signal_redirect_cb);

	get_pid_file_pathname_cb = get_pid_file_cb;
	get_threads_func_cb = get_threads_cb;
	get_threads_num_func_cb = get_threads_num_cb;

	zbx_set_common_signal_handlers(zbx_on_exit_cb_arg);

	zbx_service_start((int)flags);

	return SUCCEED;
}

void	zbx_daemon_stop(void)
{
}
