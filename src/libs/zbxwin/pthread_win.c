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

/* The subset of POSIX threads declared in include/common/zbxwinpthread.h.
 * See that header for why it exists and what it deliberately leaves out. */

#include "zbxcommon.h"

#include "zbxthreads.h"

#include <process.h>

int	pthread_mutex_init(pthread_mutex_t *mutex, const void *attr)
{
	ZBX_UNUSED(attr);
	InitializeCriticalSection(mutex);

	return 0;
}

int	pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);

	return 0;
}

int	pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(mutex);

	return 0;
}

int	pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);

	return 0;
}

int	pthread_cond_init(pthread_cond_t *cond, const void *attr)
{
	ZBX_UNUSED(attr);
	InitializeConditionVariable(cond);

	return 0;
}

int	pthread_cond_destroy(pthread_cond_t *cond)
{
	/* a condition variable holds no resource to release on Windows */
	ZBX_UNUSED(cond);

	return 0;
}

int	pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (0 == SleepConditionVariableCS(cond, mutex, INFINITE))
		return EINVAL;

	return 0;
}

int	pthread_cond_signal(pthread_cond_t *cond)
{
	WakeConditionVariable(cond);

	return 0;
}

int	pthread_cond_broadcast(pthread_cond_t *cond)
{
	WakeAllConditionVariable(cond);

	return 0;
}

int	pthread_rwlock_init(pthread_rwlock_t *rwlock, const void *attr)
{
	ZBX_UNUSED(attr);
	InitializeSRWLock(&rwlock->lock);
	rwlock->exclusive = 0;

	return 0;
}

int	pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	/* a slim reader/writer lock holds no resource to release */
	ZBX_UNUSED(rwlock);

	return 0;
}

int	pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	AcquireSRWLockShared(&rwlock->lock);

	return 0;
}

int	pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	AcquireSRWLockExclusive(&rwlock->lock);
	InterlockedExchange(&rwlock->exclusive, 1);

	return 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: releases a slim reader/writer lock                                *
 *                                                                            *
 * Comments: the mode recorded when the lock was taken selects the matching   *
 *           release call. The flag is cleared before releasing, while the    *
 *           exclusive hold still keeps every other thread out.               *
 *                                                                            *
 ******************************************************************************/
int	pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	if (0 != InterlockedCompareExchange(&rwlock->exclusive, 0, 1))
		ReleaseSRWLockExclusive(&rwlock->lock);
	else
		ReleaseSRWLockShared(&rwlock->lock);

	return 0;
}

typedef struct
{
	void	*(*start)(void *);
	void	*arg;
}
zbx_pthread_ctx_t;

/* _beginthreadex wants __stdcall and an unsigned result, POSIX wants a void
   pointer, so the entry point is wrapped */
static unsigned __stdcall	pthread_entry(void *ctx_raw)
{
	zbx_pthread_ctx_t	ctx = *(zbx_pthread_ctx_t *)ctx_raw;

	zbx_free(ctx_raw);
	ctx.start(ctx.arg);

	return 0;
}

int	pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg)
{
	zbx_pthread_ctx_t	*ctx;
	unsigned		stacksize = 0, thrdaddr;

	if (NULL != attr)
		stacksize = (unsigned)attr->stacksize;

	ctx = (zbx_pthread_ctx_t *)zbx_malloc(NULL, sizeof(zbx_pthread_ctx_t));
	ctx->start = start;
	ctx->arg = arg;

	if (0 == (*thread = (pthread_t)_beginthreadex(NULL, stacksize, pthread_entry, ctx, 0, &thrdaddr)))
	{
		zbx_free(ctx);
		return EAGAIN;
	}

	return 0;
}

int	pthread_join(pthread_t thread, void **retval)
{
	if (NULL != retval)
		*retval = NULL;

	if (WAIT_OBJECT_0 != WaitForSingleObject(thread, INFINITE))
		return EINVAL;

	CloseHandle(thread);

	return 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: sets the stack size worker threads are created with               *
 *                                                                            *
 * Comments: counterpart of the Unix implementation in zbxthreads/threads.c    *
 *           A stack size of zero lets Windows use the value from the image    *
 *           header, which is what the build already sets.                     *
 *                                                                            *
 ******************************************************************************/
void	zbx_pthread_init_attr(pthread_attr_t *attr)
{
#ifdef HAVE_STACKSIZE
	attr->stacksize = HAVE_STACKSIZE * ZBX_KIBIBYTE;
#else
	attr->stacksize = 0;
#endif
}
