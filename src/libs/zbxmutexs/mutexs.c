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

#include "zbxmutexs.h"

#ifdef _WINDOWS
#	include "zbxsysinfo.h"
#	include "zbxlog.h"
#else
#ifdef HAVE_PTHREAD_PROCESS_SHARED
typedef struct
{
	pthread_mutex_t		mutexes[ZBX_MUTEX_COUNT];
	pthread_rwlock_t	rwlocks[ZBX_RWLOCK_COUNT];
}
zbx_shared_lock_t;

static zbx_shared_lock_t	*shared_lock;
static int			shm_id, locks_disabled;
#else
#	if !HAVE_SEMUN
		union semun
		{
			int			val;	/* value for SETVAL */
			struct semid_ds		*buf;	/* buffer for IPC_STAT & IPC_SET */
			unsigned short int	*array;	/* array for GETALL & SETALL */
			struct seminfo		*__buf;	/* buffer for IPC_INFO */
		};

#		undef HAVE_SEMUN
#		define HAVE_SEMUN 1
#	endif	/* HAVE_SEMUN */

#	include "zbxcfg.h"
#	include "zbxthreads.h"

	static int		ZBX_SEM_LIST_ID = -1;
	static unsigned char	mutexes;
#endif

/******************************************************************************
 *                                                                            *
 * Purpose: if pthread mutexes and read-write locks can be shared between     *
 *          processes then create them, otherwise fallback to System V        *
 *          semaphore operations                                              *
 *                                                                            *
 * Parameters: error - [OUT] dynamically allocated memory with error message. *
 *                                                                            *
 * Return value: SUCCEED if mutexes successfully created, otherwise FAIL      *
 *                                                                            *
 ******************************************************************************/
int	zbx_locks_create(char **error)
{
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	int			i;
	pthread_mutexattr_t	mta;
	pthread_rwlockattr_t	rwa;

	if (-1 == (shm_id = shmget(IPC_PRIVATE, ZBX_SIZE_T_ALIGN8(sizeof(zbx_shared_lock_t)),
			IPC_CREAT | IPC_EXCL | 0600)))
	{
		*error = zbx_dsprintf(*error, "cannot allocate shared memory for locks");
		return FAIL;
	}

	if ((void *)(-1) == (shared_lock = (zbx_shared_lock_t *)shmat(shm_id, NULL, 0)))
	{
		*error = zbx_dsprintf(*error, "cannot attach shared memory for locks: %s", zbx_strerror(errno));
		return FAIL;
	}

	memset(shared_lock, 0, sizeof(zbx_shared_lock_t));

	/* immediately mark the new shared memory for destruction after attaching to it */
	if (-1 == shmctl(shm_id, IPC_RMID, 0))
	{
		*error = zbx_dsprintf(*error, "cannot mark the new shared memory for destruction: %s",
				zbx_strerror(errno));
		return FAIL;
	}

	if (0 != pthread_mutexattr_init(&mta))
	{
		*error = zbx_dsprintf(*error, "cannot initialize mutex attribute: %s", zbx_strerror(errno));
		return FAIL;
	}

	if (0 != pthread_mutexattr_setpshared(&mta, PTHREAD_PROCESS_SHARED))
	{
		*error = zbx_dsprintf(*error, "cannot set shared mutex attribute: %s", zbx_strerror(errno));
		return FAIL;
	}

	for (i = 0; i < ZBX_MUTEX_COUNT; i++)
	{
		if (0 != pthread_mutex_init(&shared_lock->mutexes[i], &mta))
		{
			*error = zbx_dsprintf(*error, "cannot create mutex: %s", zbx_strerror(errno));
			return FAIL;
		}
	}

	if (0 != pthread_rwlockattr_init(&rwa))
	{
		*error = zbx_dsprintf(*error, "cannot initialize read write lock attribute: %s", zbx_strerror(errno));
		return FAIL;
	}

	if (0 != pthread_rwlockattr_setpshared(&rwa, PTHREAD_PROCESS_SHARED))
	{
		*error = zbx_dsprintf(*error, "cannot set shared read write lock attribute: %s", zbx_strerror(errno));
		return FAIL;
	}

	for (i = 0; i < ZBX_RWLOCK_COUNT; i++)
	{
		if (0 != pthread_rwlock_init(&shared_lock->rwlocks[i], &rwa))
		{
			*error = zbx_dsprintf(*error, "cannot create rwlock: %s", zbx_strerror(errno));
			return FAIL;
		}
	}
#else
	union semun	semopts;
	int		i;

	if (-1 == (ZBX_SEM_LIST_ID = semget(IPC_PRIVATE, ZBX_MUTEX_COUNT + ZBX_RWLOCK_COUNT, 0600)))
	{
		*error = zbx_dsprintf(*error, "cannot create semaphore set: %s", zbx_strerror(errno));
		return FAIL;
	}

	/* set default semaphore value */

	semopts.val = 1;
	for (i = 0; ZBX_MUTEX_COUNT + ZBX_RWLOCK_COUNT > i; i++)
	{
		if (-1 != semctl(ZBX_SEM_LIST_ID, i, SETVAL, semopts))
			continue;

		*error = zbx_dsprintf(*error, "cannot initialize semaphore: %s", zbx_strerror(errno));

		if (-1 == semctl(ZBX_SEM_LIST_ID, 0, IPC_RMID, 0))
			zbx_error("cannot remove semaphore set %d: %s", ZBX_SEM_LIST_ID, zbx_strerror(errno));

		ZBX_SEM_LIST_ID = -1;

		return FAIL;
	}
#endif
	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: destroys process-shared locks                                     *
 *                                                                            *
 ******************************************************************************/
void	zbx_locks_destroy(void)
{
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	int	i;

	if (NULL == shared_lock)
		return;

	for (i = 0; i < ZBX_MUTEX_COUNT; i++)
		(void)pthread_mutex_destroy(&shared_lock->mutexes[i]);

	for (i = 0; i < ZBX_RWLOCK_COUNT; i++)
		(void)pthread_rwlock_destroy(&shared_lock->rwlocks[i]);

	if (-1 == shmdt(shared_lock))
		zabbix_log(LOG_LEVEL_TRACE, "cannot detach shared lock memory: %s", zbx_strerror(errno));

	shared_lock = NULL;
	shm_id = 0;
#else
	if (-1 == semctl(ZBX_SEM_LIST_ID, 0, IPC_RMID, 0))
		zabbix_log(LOG_LEVEL_TRACE, "cannot remove semaphore set %d: %s", ZBX_SEM_LIST_ID, zbx_strerror(errno));
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: acquire address of the mutex                                      *
 *                                                                            *
 * Parameters: mutex_name - [IN] name of the mutex to return address for      *
 *                                                                            *
 * Return value: address of the mutex                                         *
 *                                                                            *
 ******************************************************************************/
zbx_mutex_t	zbx_mutex_addr_get(zbx_mutex_name_t mutex_name)
{
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	return &shared_lock->mutexes[mutex_name];
#else
	return mutex_name;
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: acquire address of the rwlock                                     *
 *                                                                            *
 * Parameters: rwlock_name - [IN] name of the rwlock to return address for    *
 *                                                                            *
 * Return value: address of the rwlock                                        *
 *                                                                            *
 ******************************************************************************/
zbx_rwlock_t	zbx_rwlock_addr_get(zbx_rwlock_name_t rwlock_name)
{
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	return &shared_lock->rwlocks[rwlock_name];
#else
	return rwlock_name + ZBX_MUTEX_COUNT;
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: read-write locks are created using zbx_locks_create() function    *
 *          this is only to obtain handle, if read write locks are not        *
 *          supported, then outputs numeric handle of mutex that can be used  *
 *          with mutex handling functions                                     *
 *                                                                            *
 * Parameters:  rwlock - [IN/OUT] read-write lock handle if supported,        *
 *                       otherwise mutex                                      *
 *              name   - [IN] name of read-write lock (index for nix system)  *
 *              error  - [IN/OUT] unused                                      *
 *                                                                            *
 * Return value: SUCCEED if mutexes successfully created, otherwise FAIL      *
 *                                                                            *
 ******************************************************************************/
int	zbx_rwlock_create(zbx_rwlock_t *rwlock, zbx_rwlock_name_t name, char **error)
{
	ZBX_UNUSED(error);
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	*rwlock = &shared_lock->rwlocks[name];
#else
	*rwlock = name + ZBX_MUTEX_COUNT;
	mutexes++;
#endif
	return SUCCEED;
}
#ifdef HAVE_PTHREAD_PROCESS_SHARED
/******************************************************************************
 *                                                                            *
 * Purpose: acquire write lock for read-write lock (exclusive access)         *
 *                                                                            *
 * Parameters: filename - [IN] source filename (for tracking)                 *
 *             line     - [IN] source filename line number (for tracking)     *
 *             rwlock   - [IN] handle of read-write lock                      *
 *                                                                            *
 ******************************************************************************/
void	__zbx_rwlock_wrlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	if (0 != locks_disabled)
		return;

	if (0 != pthread_rwlock_wrlock(rwlock))
	{
		zbx_error("[file:'%s',line:%d] write lock failed: %s", filename, line, zbx_strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: acquire read lock for read-write lock (there can be many readers) *
 *                                                                            *
 * Parameters: filename - [IN] source filename (for tracking)                 *
 *             line     - [IN] source filename line number (for tracking)     *
 *             rwlock   - [IN] handle of read-write lock                      *
 *                                                                            *
 ******************************************************************************/
void	__zbx_rwlock_rdlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	if (0 != locks_disabled)
		return;

	if (0 != pthread_rwlock_rdlock(rwlock))
	{
		zbx_error("[file:'%s',line:%d] read lock failed: %s", filename, line, zbx_strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: unlock read-write lock                                            *
 *                                                                            *
 * Parameters: filename - [IN] source filename (for tracking)                 *
 *             line     - [IN] source filename line number (for tracking)     *
 *             rwlock   - [IN] handle of read-write lock                      *
 *                                                                            *
 ******************************************************************************/
void	__zbx_rwlock_unlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	if (0 != locks_disabled)
		return;

	if (0 != pthread_rwlock_unlock(rwlock))
	{
		zbx_error("[file:'%s',line:%d] read-write lock unlock failed: %s", filename, line, zbx_strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: Destroy read-write lock                                           *
 *                                                                            *
 * Parameters: rwlock - [IN] handle of read-write lock                        *
 *                                                                            *
 ******************************************************************************/
void	zbx_rwlock_destroy(zbx_rwlock_t *rwlock)
{
	if (ZBX_RWLOCK_NULL == *rwlock)
		return;

	*rwlock = ZBX_RWLOCK_NULL;
}

/******************************************************************************
 *                                                                            *
 * Purpose:  disable locks                                                    *
 *                                                                            *
 ******************************************************************************/
void	zbx_locks_disable(void)
{
	/* attempting to destroy a locked pthread mutex results in undefined behavior */
	locks_disabled = 1;
}

/******************************************************************************
 *                                                                            *
 * Purpose:  enable locks                                                     *
 *                                                                            *
 ******************************************************************************/
void	zbx_locks_enable(void)
{
	/* attempting to destroy a locked pthread mutex results in undefined behavior */
	locks_disabled = 0;
}

#endif
#endif	/* _WINDOWS */

#ifdef _WINDOWS
/******************************************************************************
 *                                                                            *
 * Purpose: creates process-shared locks                                      *
 *                                                                            *
 * Comments: Windows workers are threads of a single process, and             *
 *           zbx_mutex_create() and zbx_rwlock_create() build their objects   *
 *           on demand, so there is no pool to set up in advance.             *
 *                                                                            *
 ******************************************************************************/
int	zbx_locks_create(char **error)
{
	ZBX_UNUSED(error);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: destroys process-shared locks                                     *
 *                                                                            *
 ******************************************************************************/
void	zbx_locks_destroy(void)
{
}

/* On Unix the workers fork and share these mutexes through a System V segment; here they are threads of one process, */
/* so an in-process lock is enough. The port first used named kernel mutexes (CreateMutex): one kernel transition per */
/* acquire and per release, fine uncontended but a lock convoy once several trapper threads write at once, which is   */
/* why multi-sender ingest ran slower than a single sender. A CRITICAL_SECTION spins briefly in user space and enters */
/* the kernel only when it has to wait - what the concurrent path needs.                                             */
/*                                                                                                                    */
/* A mutex is still identified by name: two translation units create ZBX_MUTEX_CACHE_IDS on their own and must end up */
/* with the same lock. Named mutexes therefore live in a small registry keyed by name, and a second create of a name  */
/* already present returns the same lock with a raised reference count. A NULL name (the OpenSSL per-lock array) is   */
/* anonymous: always a fresh, unshared lock, never registered. The registry itself is guarded by its own critical    */
/* section, brought up once however early the first mutex is created.                                                 */

#define ZBX_WIN_MUTEX_SPIN	4000	/* user-space spins before a contended wait enters the kernel */
#define ZBX_WIN_MUTEX_MAX	128	/* distinct named mutexes: the enum names plus dynamic shared-memory segments */

struct zbx_win_mutex
{
	CRITICAL_SECTION	cs;
	wchar_t			*name;		/* NULL for an anonymous lock, which the registry never holds */
	int			refcount;	/* named locks: shared creates and the matching destroys */
};

static struct zbx_win_mutex	*win_mutex_registry[ZBX_WIN_MUTEX_MAX];
static int			win_mutex_registry_num;
static CRITICAL_SECTION		win_mutex_registry_cs;
static INIT_ONCE		win_mutex_registry_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK	win_mutex_registry_init(PINIT_ONCE once, PVOID param, PVOID *context)
{
	ZBX_UNUSED(once);
	ZBX_UNUSED(param);
	ZBX_UNUSED(context);

	InitializeCriticalSectionAndSpinCount(&win_mutex_registry_cs, ZBX_WIN_MUTEX_SPIN);

	return TRUE;
}

static struct zbx_win_mutex	*win_mutex_alloc(const wchar_t *name)
{
	struct zbx_win_mutex	*m;

	m = (struct zbx_win_mutex *)zbx_malloc(NULL, sizeof(struct zbx_win_mutex));
	InitializeCriticalSectionAndSpinCount(&m->cs, ZBX_WIN_MUTEX_SPIN);
	m->refcount = 1;

	if (NULL == name)
	{
		m->name = NULL;
	}
	else
	{
		size_t	len = wcslen(name) + 1;

		m->name = (wchar_t *)zbx_malloc(NULL, len * sizeof(wchar_t));
		memcpy(m->name, name, len * sizeof(wchar_t));
	}

	return m;
}
#endif	/* _WINDOWS */

/******************************************************************************
 *                                                                            *
 * Purpose: Create the mutex                                                  *
 *                                                                            *
 * Parameters:  mutex - [IN/OUT] handle of mutex                              *
 *              name  - [IN] name of mutex (index for nix system)             *
 *              error - [OUT] the error message                               *
 *                                                                            *
 * Return value: If the function succeeds, then return SUCCEED,               *
 *               FAIL on an error                                             *
 *                                                                            *
 ******************************************************************************/
int	zbx_mutex_create(zbx_mutex_t *mutex, zbx_mutex_name_t name, char **error)
{
#ifdef _WINDOWS
	ZBX_UNUSED(error);

	if (NULL == name)
	{
		/* anonymous lock: never shared, so it is not registered */
		*mutex = (zbx_mutex_t)win_mutex_alloc(NULL);
	}
	else
	{
		struct zbx_win_mutex	*m = NULL;
		int			i;

		InitOnceExecuteOnce(&win_mutex_registry_once, win_mutex_registry_init, NULL, NULL);
		EnterCriticalSection(&win_mutex_registry_cs);

		for (i = 0; i < win_mutex_registry_num; i++)
		{
			if (0 == wcscmp(win_mutex_registry[i]->name, name))
			{
				m = win_mutex_registry[i];
				m->refcount++;
				break;
			}
		}

		if (NULL == m)
		{
			if (ZBX_WIN_MUTEX_MAX == win_mutex_registry_num)
			{
				LeaveCriticalSection(&win_mutex_registry_cs);
				THIS_SHOULD_NEVER_HAPPEN;
				exit(EXIT_FAILURE);
			}

			m = win_mutex_alloc(name);
			win_mutex_registry[win_mutex_registry_num++] = m;
		}

		LeaveCriticalSection(&win_mutex_registry_cs);
		*mutex = (zbx_mutex_t)m;
	}
#else
	ZBX_UNUSED(error);
#ifdef	HAVE_PTHREAD_PROCESS_SHARED
	*mutex = &shared_lock->mutexes[name];
#else
	mutexes++;
	*mutex = name;
#endif
#endif
	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Waits until the mutex is in the signalled state                   *
 *                                                                            *
 * Parameters: filename - [IN] source filename (for tracking)                 *
 *             line     - [IN] source filename line number (for tracking)     *
 *             mutex    - [IN] handle of read-write lock                      *
 *                                                                            *
 ******************************************************************************/
void	__zbx_mutex_lock(const char *filename, int line, zbx_mutex_t mutex)
{
#ifndef _WINDOWS
#ifndef	HAVE_PTHREAD_PROCESS_SHARED
	struct sembuf	sem_lock;
#endif
#endif

	if (ZBX_MUTEX_NULL == mutex)
		return;

#ifdef _WINDOWS
#ifdef ZABBIX_AGENT
	if (0 != (ZBX_MUTEX_THREAD_DENIED & zbx_get_thread_global_mutex_flag()))
	{
		zbx_error("[file:'%s',line:%d] lock failed: ZBX_MUTEX_THREAD_DENIED is set for thread with id = %d",
				filename, line, zbx_get_thread_id());
		exit(EXIT_FAILURE);
	}
#else
	ZBX_UNUSED(filename);
	ZBX_UNUSED(line);
#endif
	EnterCriticalSection(&((struct zbx_win_mutex *)mutex)->cs);
#else
#ifdef	HAVE_PTHREAD_PROCESS_SHARED
	if (0 != locks_disabled)
		return;

	if (0 != pthread_mutex_lock(mutex))
	{
		zbx_error("[file:'%s',line:%d] lock failed: %s", filename, line, zbx_strerror(errno));
		exit(EXIT_FAILURE);
	}
#else
	sem_lock.sem_num = mutex;
	sem_lock.sem_op = -1;
	sem_lock.sem_flg = SEM_UNDO;

	while (-1 == semop(ZBX_SEM_LIST_ID, &sem_lock, 1))
	{
		if (EINTR != errno)
		{
			zbx_error("[file:'%s',line:%d] lock failed: %s", filename, line, zbx_strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#endif
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: Unlock the mutex                                                  *
 *                                                                            *
 * Parameters: filename - [IN] source filename (for tracking)                 *
 *             line     - [IN] source filename line number (for tracking)     *
 *             mutex    - [IN] handle of read-write lock                      *
 *                                                                            *
 ******************************************************************************/
void	__zbx_mutex_unlock(const char *filename, int line, zbx_mutex_t mutex)
{
#ifndef _WINDOWS
#ifndef	HAVE_PTHREAD_PROCESS_SHARED
	struct sembuf	sem_unlock;
#endif
#endif

	if (ZBX_MUTEX_NULL == mutex)
		return;

#ifdef _WINDOWS
	ZBX_UNUSED(filename);
	ZBX_UNUSED(line);

	LeaveCriticalSection(&((struct zbx_win_mutex *)mutex)->cs);
#else
#ifdef	HAVE_PTHREAD_PROCESS_SHARED
	if (0 != locks_disabled)
		return;

	if (0 != pthread_mutex_unlock(mutex))
	{
		zbx_error("[file:'%s',line:%d] unlock failed: %s", filename, line, zbx_strerror(errno));
		exit(EXIT_FAILURE);
	}
#else
	sem_unlock.sem_num = mutex;
	sem_unlock.sem_op = 1;
	sem_unlock.sem_flg = SEM_UNDO;

	while (-1 == semop(ZBX_SEM_LIST_ID, &sem_unlock, 1))
	{
		if (EINTR != errno)
		{
			zbx_error("[file:'%s',line:%d] unlock failed: %s", filename, line, zbx_strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#endif
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: Destroy the mutex                                                 *
 *                                                                            *
 * Parameters: mutex - [IN/OUT] handle of mutex                               *
 *                                                                            *
 ******************************************************************************/
void	zbx_mutex_destroy(zbx_mutex_t *mutex)
{
#ifdef _WINDOWS
	struct zbx_win_mutex	*m = (struct zbx_win_mutex *)*mutex;

	if (ZBX_MUTEX_NULL == m)
		return;

	if (NULL == m->name)
	{
		/* anonymous lock: owned by its one caller, not in the registry */
		DeleteCriticalSection(&m->cs);
		zbx_free(m);
	}
	else
	{
		InitOnceExecuteOnce(&win_mutex_registry_once, win_mutex_registry_init, NULL, NULL);
		EnterCriticalSection(&win_mutex_registry_cs);

		/* the last holder frees it; the others just drop their reference */
		if (0 == --m->refcount)
		{
			int	i;

			for (i = 0; i < win_mutex_registry_num; i++)
			{
				if (win_mutex_registry[i] == m)
				{
					win_mutex_registry[i] = win_mutex_registry[--win_mutex_registry_num];
					break;
				}
			}

			DeleteCriticalSection(&m->cs);
			zbx_free(m->name);
			zbx_free(m);
		}

		LeaveCriticalSection(&win_mutex_registry_cs);
	}
#endif
	*mutex = ZBX_MUTEX_NULL;
}

#ifdef _WINDOWS
/******************************************************************************
 *                                                                            *
 * Purpose: Appends PID to the prefix of the mutex                            *
 *                                                                            *
 * Parameters: prefix - [IN] mutex type                                       *
 *                                                                            *
 * Return value: Dynamically allocated, NUL terminated name of the mutex      *
 *                                                                            *
 * Comments: The mutex name must be shorter than MAX_PATH characters,         *
 *           otherwise the function calls exit()                              *
 *                                                                            *
 ******************************************************************************/
zbx_mutex_name_t	zbx_mutex_create_per_process_name(const zbx_mutex_name_t prefix)
{
	zbx_mutex_name_t	name = ZBX_MUTEX_NULL;
	int			size;
	wchar_t			*format = L"%s_PID_%lx";
	DWORD			pid = GetCurrentProcessId();

	/* exit if the mutex name length exceed the maximum allowed */
	size = _scwprintf(format, prefix, pid);
	if (MAX_PATH < size)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	size = size + 1; /* for terminating '\0' */

	name = zbx_malloc(NULL, sizeof(wchar_t) * size);
	(void)_snwprintf_s(name, size, size - 1, format, prefix, pid);
	name[size - 1] = L'\0';

	return name;
}
#endif

#ifdef _WINDOWS
/* Read-write locks. Elsewhere these are process-shared pthread locks living in a shared memory segment. On Windows */
/* every Zabbix worker is a thread of one process, so a plain slim reader/writer lock in static storage covers the */
/* same ground. SRWLOCK_INIT is all zeroes, which static storage already is, so the table needs no initialisation. */
/* SRWLOCK is used directly rather than through the POSIX subset in src/libs/zbxwin, because this translation unit is */
/* also linked into the agent, which does not build that library. */

struct zbx_win_rwlock
{
	SRWLOCK	lock;
};

static struct zbx_win_rwlock	rwlocks[ZBX_RWLOCK_COUNT];

/* Windows releases the shared and the exclusive mode with different calls and cannot be asked which one is held, so */
/* the mode has to be recorded. It cannot be recorded on the lock: the lock is shared between threads, the answer is */
/* not - while one thread holds the lock exclusively another may be about to release the shared hold it took before, */
/* and a single flag tells them both the same thing. Releasing in the wrong mode raises STATUS_RESOURCE_NOT_OWNED and */
/* takes the process down. Each thread therefore keeps its own record, which no other thread can disturb. The depth */
/* counts nested holds, which the readers of the configuration cache do take. */
#define ZBX_RWLOCK_HELD_SHARED		1
#define ZBX_RWLOCK_HELD_EXCLUSIVE	2

static ZBX_THREAD_LOCAL unsigned char	rwlock_held[ZBX_RWLOCK_COUNT];
static ZBX_THREAD_LOCAL int		rwlock_depth[ZBX_RWLOCK_COUNT];

int	zbx_rwlock_create(zbx_rwlock_t *rwlock, zbx_rwlock_name_t name, char **error)
{
	ZBX_UNUSED(error);

	*rwlock = &rwlocks[name];

	return SUCCEED;
}

zbx_rwlock_t	zbx_rwlock_addr_get(zbx_rwlock_name_t rwlock_name)
{
	return &rwlocks[rwlock_name];
}

void	__zbx_rwlock_wrlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	int	index;

	ZBX_UNUSED(filename);
	ZBX_UNUSED(line);

	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	AcquireSRWLockExclusive(&rwlock->lock);

	index = (int)(rwlock - rwlocks);
	rwlock_held[index] = ZBX_RWLOCK_HELD_EXCLUSIVE;
	rwlock_depth[index]++;
}

void	__zbx_rwlock_rdlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	int	index;

	ZBX_UNUSED(filename);
	ZBX_UNUSED(line);

	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	AcquireSRWLockShared(&rwlock->lock);

	index = (int)(rwlock - rwlocks);
	rwlock_held[index] = ZBX_RWLOCK_HELD_SHARED;
	rwlock_depth[index]++;
}

void	__zbx_rwlock_unlock(const char *filename, int line, zbx_rwlock_t rwlock)
{
	int	index;

	if (ZBX_RWLOCK_NULL == rwlock)
		return;

	index = (int)(rwlock - rwlocks);

	if (0 == rwlock_depth[index])
	{
		/* releasing a lock this thread never took would corrupt the lock for */
		/* everyone waiting on it, so say where it came from and do nothing   */
		zbx_error("[file:'%s',line:%d] read-write lock released by a thread that does not hold it",
				filename, line);
		return;
	}

	if (ZBX_RWLOCK_HELD_EXCLUSIVE == rwlock_held[index])
		ReleaseSRWLockExclusive(&rwlock->lock);
	else
		ReleaseSRWLockShared(&rwlock->lock);

	if (0 == --rwlock_depth[index])
		rwlock_held[index] = 0;
}

void	zbx_rwlock_destroy(zbx_rwlock_t *rwlock)
{
	/* a slim reader/writer lock holds no resource to release */
	*rwlock = ZBX_RWLOCK_NULL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: suspend and resume locking                                        *
 *                                                                            *
 * Comments: the Unix implementation uses this after fork(), where a child     *
 *           may inherit a lock held by another thread of the parent. There    *
 *           is no fork here, so there is nothing to suspend.                  *
 *                                                                            *
 ******************************************************************************/
void	zbx_locks_disable(void)
{
}

void	zbx_locks_enable(void)
{
}
#endif	/* _WINDOWS */
