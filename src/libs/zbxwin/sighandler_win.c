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

/* Windows counterpart of src/libs/zbxnix/sighandler.c. Termination is driven by the service control handler and by */
/* console control events, both of which live in src/libs/zbxwinservice/service.c and end up calling ZBX_DO_EXIT(). */
/* That gives a running/stopped flag but no exit status, which the proxy needs, so the status is tracked here on top */
/* of it: ZBX_IS_RUNNING() stays the one in service.c, ZBX_EXIT_STATUS() is ours. There are no POSIX signals to */
/* block. The signal-set operations are kept as no-ops rather than removed, so that their twenty-odd callers across */
/* the shared libraries compile unchanged. */

#include "zbxcommon.h"

#include "zbxnix.h"
#include "zbxwinservice.h"
#include "zbxlog.h"

#define ZBX_EXIT_NONE		0
#define ZBX_EXIT_SUCCESS	1
#define ZBX_EXIT_FAILURE	2

static volatile LONG	sig_exiting = ZBX_EXIT_NONE;

static zbx_on_exit_t	zbx_on_exit_cb = NULL;
static void		*zbx_on_exit_args = NULL;

static ZBX_THREAD_HANDLE	*child_threads = NULL;
static size_t			child_threads_num = 0;

static zbx_get_progname_f		get_progname_func_cb = NULL;
static zbx_get_process_info_by_thread_f	get_process_info_by_thread_func_cb = NULL;

void	zbx_set_exiting_with_fail(void)
{
	InterlockedExchange(&sig_exiting, ZBX_EXIT_FAILURE);
	ZBX_DO_EXIT();
}

void	zbx_set_exiting_with_succeed(void)
{
	InterlockedExchange(&sig_exiting, ZBX_EXIT_SUCCESS);
	ZBX_DO_EXIT();
}

int	ZBX_EXIT_STATUS(void)
{
	return ZBX_EXIT_FAILURE == InterlockedCompareExchange(&sig_exiting, 0, 0) ? FAIL : SUCCEED;
}

int	zbx_init_thread_signal_handler(void)
{
	/* worker threads inherit the process-wide console and service handlers */
	return SUCCEED;
}

void	zbx_set_common_signal_handlers(zbx_on_exit_t zbx_on_exit_cb_arg)
{
	zbx_on_exit_cb = zbx_on_exit_cb_arg;
	zbx_set_parent_signal_handler(zbx_on_exit_cb_arg);
}

void	zbx_set_on_exit_args(void *args)
{
	zbx_on_exit_args = args;
}

void	zbx_set_child_pids(ZBX_THREAD_HANDLE *pids, size_t pid_num)
{
	child_threads = pids;
	child_threads_num = pid_num;
}

void	zbx_log_exit_signal(void)
{
	if (ZBX_EXIT_NONE != InterlockedCompareExchange(&sig_exiting, 0, 0))
		zabbix_log(LOG_LEVEL_INFORMATION, "Got shutdown request, exiting...");
}

/* the operations below have no Windows counterpart and exist so that the shared libraries compile and link unchanged */

void	zbx_set_child_signal_handler(void)
{
}

void	zbx_unset_child_signal_handler(void)
{
}

void	zbx_set_metric_thread_signal_handler(void)
{
}

void	zbx_block_signals(sigset_t *orig_mask)
{
	ZBX_UNUSED(orig_mask);
}

void	zbx_block_thread_signals(sigset_t *orig_mask)
{
	ZBX_UNUSED(orig_mask);
}

void	zbx_unblock_signals(const sigset_t *orig_mask)
{
	ZBX_UNUSED(orig_mask);
}

void	zbx_set_exit_on_terminate(void)
{
}

void	zbx_unset_exit_on_terminate(void)
{
}

void	zbx_init_library_nix(zbx_get_progname_f get_progname_cb, zbx_get_process_info_by_thread_f
		get_process_info_by_thread_cb)
{
	get_progname_func_cb = get_progname_cb;
	get_process_info_by_thread_func_cb = get_process_info_by_thread_cb;
}

/* accessors used by the other translation units of this library */

zbx_get_progname_f	win_get_progname_cb(void)
{
	return get_progname_func_cb;
}

zbx_get_process_info_by_thread_f	win_get_process_info_by_thread_func_cb(void)
{
	return get_process_info_by_thread_func_cb;
}

ZBX_THREAD_HANDLE	*win_get_child_threads(size_t *num)
{
	*num = child_threads_num;
	return child_threads;
}

void	win_call_on_exit(int ret)
{
	if (NULL != zbx_on_exit_cb)
		zbx_on_exit_cb(ret, zbx_on_exit_args);
}
