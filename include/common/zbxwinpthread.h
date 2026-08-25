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

#ifndef ZABBIX_WINPTHREAD_H
#define ZABBIX_WINPTHREAD_H

#ifndef _WINDOWS
#	error "This module is only available for Windows OS"
#endif

/* The discoverer, the asynchronous pollers and the preprocessing manager run their own worker pools, and each */
/* synchronises them with POSIX threads directly rather than through the Zabbix thread interface. Those primitives */
/* are all intra-process and map one to one onto Windows: pthread_mutex_t -> CRITICAL_SECTION pthread_cond_t -> */
/* CONDITION_VARIABLE pthread_rwlock_t -> SRWLOCK pthread_t -> thread handle from _beginthreadex() so this provides */
/* the subset they use rather than reshaping three worker pools. Only the intra-process case is covered: the */
/* process-shared attributes that zbxmutexs uses have no counterpart here and are not declared. The condition */
/* variable and the slim reader/writer lock require Windows Vista, which is why the proxy raises _WIN32_WINNT past */
/* the value the agent targets. */

#include <windows.h>

typedef CRITICAL_SECTION	pthread_mutex_t;
typedef CONDITION_VARIABLE	pthread_cond_t;
typedef HANDLE			pthread_t;

/* Windows has separate release calls for the shared and the exclusive mode, while POSIX has one, and a slim lock */
/* cannot be asked which mode it is held in. The mode is therefore recorded when the lock is taken. One flag is */
/* enough: an exclusive holder excludes every other holder, so it cannot be observed at the same time as a shared */
/* one. */
typedef struct
{
	SRWLOCK		lock;
	volatile LONG	exclusive;
}
pthread_rwlock_t;

typedef struct
{
	size_t	stacksize;
}
pthread_attr_t;

/* the POSIX functions return 0 on success and an errno value on failure, which the callers log through */
/* zbx_strerror(); that convention is kept */

int	pthread_mutex_init(pthread_mutex_t *mutex, const void *attr);
int	pthread_mutex_destroy(pthread_mutex_t *mutex);
int	pthread_mutex_lock(pthread_mutex_t *mutex);
int	pthread_mutex_unlock(pthread_mutex_t *mutex);

int	pthread_cond_init(pthread_cond_t *cond, const void *attr);
int	pthread_cond_destroy(pthread_cond_t *cond);
int	pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int	pthread_cond_signal(pthread_cond_t *cond);
int	pthread_cond_broadcast(pthread_cond_t *cond);

int	pthread_rwlock_init(pthread_rwlock_t *rwlock, const void *attr);
int	pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int	pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

int	pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg);
int	pthread_join(pthread_t thread, void **retval);

#endif /* ZABBIX_WINPTHREAD_H */
