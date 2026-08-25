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

/* Windows counterpart of src/libs/zbxnix/control.c. Runtime control reaches a worker over the IPC service; */
/* rtc_service.c only falls back to these functions for a worker that has no IPC subscription, which on Unix it */
/* reaches by signalling its PID. That fallback has no equivalent here. The workers are threads of one process, so */
/* there is no per-worker process to signal, and Windows offers no way to interrupt one thread with a request the way */
/* a signal does. Rather than pretend the request was delivered, these report that it was not, so an operator sees */
/* why the option had no effect. */

#include "zbxcommon.h"

#include "zbxnix.h"
#include "zbxstr.h"

static void	(*sigusr_handler_cb)(int flags) = NULL;

/* The service has notified every worker subscribed to it by the time these are reached, so what is left to report is */
/* the remainder, not an outright failure. */
static const char	*control_unsupported =
		"Workers subscribed to the runtime control service have been notified. The rest are"
		" threads of this process and cannot be addressed individually, so they keep their"
		" current setting.\n";

void	zbx_signal_process_by_type(int proc_type, int proc_num, int flags, char **out)
{
	ZBX_UNUSED(proc_type);
	ZBX_UNUSED(proc_num);
	ZBX_UNUSED(flags);

	if (NULL != out)
		*out = zbx_strdup(*out, control_unsupported);
}

void	zbx_signal_process_by_pid(int pid, int flags, char **out)
{
	ZBX_UNUSED(pid);
	ZBX_UNUSED(flags);

	if (NULL != out)
		*out = zbx_strdup(*out, control_unsupported);
}

void	zbx_set_sigusr_handler(void (*handler)(int flags))
{
	sigusr_handler_cb = handler;
}

/******************************************************************************
 *                                                                            *
 * Purpose: forwards a runtime control request to a running instance          *
 *                                                                            *
 * Comments: the Unix implementation reads a PID file and signals the daemon. *
 *           The Windows proxy is reached over the runtime control service    *
 *           instead, which the caller uses before falling back here.         *
 *                                                                            *
 ******************************************************************************/
int	zbx_sigusr_send(int flags, const char *pid_file_pathname)
{
	ZBX_UNUSED(flags);
	ZBX_UNUSED(pid_file_pathname);

	zbx_error("cannot send the runtime control request: not supported on Windows");

	return FAIL;
}
