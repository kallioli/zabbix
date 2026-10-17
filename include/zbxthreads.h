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

#ifndef ZABBIX_THREADS_H
#define ZABBIX_THREADS_H

#include "zbxcommon.h"

/* the order workers are asked to stop in, on either platform */
#define ZBX_THREAD_PRIORITY_NONE	0
#define ZBX_THREAD_PRIORITY_COLLECTOR	1
#define ZBX_THREAD_PRIORITY_SYNCER	2
#define ZBX_THREAD_PRIORITY_WORKER	3
#define ZBX_THREAD_PRIORITY_COUNT	4

#if defined(_WINDOWS) || defined(__MINGW32__)
	/* the ZBXEndThread function is implemented in service.c file */
	void	CALLBACK ZBXEndThread(ULONG_PTR dwParam);

	#define ZBX_THREAD_ERROR		0

	#define ZBX_THREAD_HANDLE		HANDLE
	#define ZBX_THREAD_HANDLE_NULL		NULL

	#define ZBX_THREAD_ENTRY_POINTER(pointer_name)		\
		unsigned (__stdcall *pointer_name)(void *)

	#define ZBX_THREAD_ENTRY(entry_name, arg_name)		\
		unsigned __stdcall entry_name(void *arg_name)

	#define zbx_thread_exit(status)				\
		_endthreadex((unsigned int)(status));		\
		return ((unsigned)(status))

	#define zbx_sleep(sec)			SleepEx(((DWORD)(sec)) * ((DWORD)1000), TRUE)

	#define zbx_thread_kill(h)		QueueUserAPC(ZBXEndThread, h, 0)
	#define zbx_thread_kill_fatal(h)	QueueUserAPC(ZBXEndThread, h, 0)
#else	/* not _WINDOWS */

	int	zbx_fork(void);
	void	zbx_child_fork(pid_t *pid);
	int	zbx_child_cleanup(pid_t pid, pid_t *threads, size_t threads_num);

	#define ZBX_THREAD_ERROR		-1

	#define ZBX_THREAD_HANDLE		pid_t
	#define ZBX_THREAD_HANDLE_NULL		0


	#define ZBX_THREAD_ENTRY_POINTER(pointer_name)	\
		unsigned (* pointer_name)(void *)

	#define ZBX_THREAD_ENTRY(entry_name, arg_name)	\
		unsigned entry_name(void *arg_name)

	/* Calling _exit() to terminate child process immediately is important. See ZBX-5732 for details. */
	#define zbx_thread_exit(status)			\
		_exit((int)(status));			\
		return ((unsigned)(status))

	#define zbx_sleep(sec)			sleep((sec))

	#define zbx_thread_kill(h)		kill(h, SIGUSR2)
	#define zbx_thread_kill_fatal(h)	kill(h, SIGHUP)
#endif	/* _WINDOWS */

/* Ends a worker that has finished on its own: a process on Unix, where this is exit(), and a thread on      */
/* Windows, where exit() would end the whole program, every other worker and the service with it. Unlike      */
/* zbx_thread_exit() it keeps exit()'s semantics on Unix and can be used at any depth of the worker, as both   */
/* forms are declared noreturn.                                                                                */
#if defined(_WINDOWS) || defined(__MINGW32__)
	#define zbx_worker_exit(status)		_endthreadex((unsigned int)(status))
#else
	#define zbx_worker_exit(status)		exit((int)(status))
#endif

typedef struct
{
	unsigned char	program_type;
	int		server_num;
	int		process_num;
	unsigned char	process_type;
}
zbx_thread_info_t;

typedef struct
{
	zbx_thread_info_t	info;
	void			*args;
#if defined(_WINDOWS) || defined(__MINGW32__)
	ZBX_THREAD_ENTRY_POINTER(entry);
#endif
}
zbx_thread_args_t;

void	zbx_thread_start(ZBX_THREAD_ENTRY_POINTER(handler), zbx_thread_args_t *thread_args, ZBX_THREAD_HANDLE *thread);
int	zbx_thread_wait(ZBX_THREAD_HANDLE thread);
void	zbx_threads_kill_and_wait(ZBX_THREAD_HANDLE *threads, const int *threads_flags, int threads_num, int ret);

void	zbx_pthread_init_attr(pthread_attr_t *attr);

#endif	/* ZABBIX_THREADS_H */
