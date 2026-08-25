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

/* Windows counterpart of src/libs/zbxnix/ipc.c and src/libs/zbxnix/dshm.c. zbx_thread_start() creates a thread on */
/* Windows where it forks on Unix, so every Zabbix worker shares one address space. What System V shared memory buys */
/* on Unix - a region visible from several processes - is therefore already true of ordinary heap memory here, and */
/* these segments are plain allocations tracked by identifier so that callers keep working with shmids. The */
/* reallocation path is correspondingly simpler than dshm.c: there are no other processes holding a stale attachment, */
/* so growing a segment is a copy into a new allocation, with no re-attachment dance. */

#include "zbxcommon.h"

#include "zbxnix.h"
#include "zbxmutexs.h"

#define ZBX_SHM_SEGMENTS_MAX	64

typedef struct
{
	void	*addr;
	size_t	size;
}
zbx_shm_segment_t;

/* indexed by shmid; a NULL address marks a free slot */
static zbx_shm_segment_t	shm_segments[ZBX_SHM_SEGMENTS_MAX];
static CRITICAL_SECTION		shm_lock;
static volatile LONG		shm_lock_ready;

static void	shm_lock_acquire(void)
{
	/* the first caller wins the initialisation, the others wait for it */
	if (0 == InterlockedCompareExchange(&shm_lock_ready, 1, 0))
	{
		InitializeCriticalSection(&shm_lock);
		InterlockedExchange(&shm_lock_ready, 2);
	}
	else
	{
		while (2 != InterlockedCompareExchange(&shm_lock_ready, 2, 2))
			SwitchToThread();
	}

	EnterCriticalSection(&shm_lock);
}

static void	shm_lock_release(void)
{
	LeaveCriticalSection(&shm_lock);
}

/******************************************************************************
 *                                                                            *
 * Purpose: allocates a shared memory segment                                 *
 *                                                                            *
 * Parameters: size - [IN] size of the segment                                *
 *                                                                            *
 * Return value: segment identifier, or -1 on error                           *
 *                                                                            *
 ******************************************************************************/
int	zbx_shm_create(size_t size)
{
	int	shmid = -1, i;
	void	*addr;

	if (NULL == (addr = zbx_malloc(NULL, size)))
		return -1;

	memset(addr, 0, size);

	shm_lock_acquire();

	for (i = 0; i < ZBX_SHM_SEGMENTS_MAX; i++)
	{
		if (NULL == shm_segments[i].addr)
		{
			shm_segments[i].addr = addr;
			shm_segments[i].size = size;
			shmid = i;
			break;
		}
	}

	shm_lock_release();

	if (-1 == shmid)
	{
		zbx_free(addr);
		zabbix_log(LOG_LEVEL_CRIT, "cannot create shared memory segment: the limit of %d is reached",
				ZBX_SHM_SEGMENTS_MAX);
	}

	return shmid;
}

/******************************************************************************
 *                                                                            *
 * Purpose: frees a shared memory segment                                     *
 *                                                                            *
 * Parameters: shmid - [IN] segment identifier                                *
 *                                                                            *
 * Return value: SUCCEED or FAIL                                              *
 *                                                                            *
 ******************************************************************************/
int	zbx_shm_destroy(int shmid)
{
	void	*addr = NULL;

	if (0 > shmid || ZBX_SHM_SEGMENTS_MAX <= shmid)
		return FAIL;

	shm_lock_acquire();
	addr = shm_segments[shmid].addr;
	shm_segments[shmid].addr = NULL;
	shm_segments[shmid].size = 0;
	shm_lock_release();

	if (NULL == addr)
		return FAIL;

	zbx_free(addr);

	return SUCCEED;
}

static void	*shm_addr(int shmid)
{
	void	*addr;

	if (0 > shmid || ZBX_SHM_SEGMENTS_MAX <= shmid)
		return NULL;

	shm_lock_acquire();
	addr = shm_segments[shmid].addr;
	shm_lock_release();

	return addr;
}

int	zbx_dshm_create(zbx_dshm_t *shm, size_t shm_size, zbx_mutex_name_t mutex,
		zbx_shm_copy_func_t copy_func, char **errmsg)
{
	int	ret = FAIL;

	if (SUCCEED != zbx_mutex_create(&shm->lock, mutex, errmsg))
		goto out;

	if (0 < shm_size)
	{
		if (-1 == (shm->shmid = zbx_shm_create(shm_size)))
		{
			*errmsg = zbx_strdup(*errmsg, "cannot allocate shared memory");
			goto out;
		}
	}
	else
		shm->shmid = ZBX_NONEXISTENT_SHMID;

	shm->size = shm_size;
	shm->copy_func = copy_func;

	ret = SUCCEED;
out:
	return ret;
}

int	zbx_dshm_destroy(zbx_dshm_t *shm, char **errmsg)
{
	int	ret = FAIL;

	zbx_mutex_destroy(&shm->lock);

	if (ZBX_NONEXISTENT_SHMID != shm->shmid)
	{
		if (SUCCEED != zbx_shm_destroy(shm->shmid))
		{
			*errmsg = zbx_strdup(*errmsg, "cannot free shared memory");
			goto out;
		}

		shm->shmid = ZBX_NONEXISTENT_SHMID;
	}

	ret = SUCCEED;
out:
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: resizes a dynamic shared memory segment                           *
 *                                                                            *
 * Comments: unlike the Unix implementation there is no other process holding *
 *           the old segment, so this is an allocate-copy-free sequence.       *
 *                                                                            *
 ******************************************************************************/
int	zbx_dshm_realloc(zbx_dshm_t *shm, size_t size, char **errmsg)
{
	int	shmid_old = shm->shmid, ret = FAIL;
	void	*addr_old, *addr;
	size_t	shm_size;

	shm_size = ZBX_SIZE_T_ALIGN8(size);

	if (-1 == (shm->shmid = zbx_shm_create(shm_size)))
	{
		*errmsg = zbx_strdup(*errmsg, "cannot allocate shared memory");
		shm->shmid = shmid_old;
		goto out;
	}

	if (NULL == (addr = shm_addr(shm->shmid)))
	{
		*errmsg = zbx_strdup(*errmsg, "cannot find the new shared memory segment");
		goto out;
	}

	addr_old = (ZBX_NONEXISTENT_SHMID != shmid_old ? shm_addr(shmid_old) : NULL);

	shm->copy_func(addr, shm_size, addr_old);

	if (ZBX_NONEXISTENT_SHMID != shmid_old && SUCCEED != zbx_shm_destroy(shmid_old))
	{
		*errmsg = zbx_strdup(*errmsg, "cannot free the old shared memory segment");
		goto out;
	}

	shm->size = shm_size;

	ret = SUCCEED;
out:
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: refreshes a local reference to a shared memory segment            *
 *                                                                            *
 * Comments: the address never changes behind the caller's back here, so this *
 *           only has to fill in a reference that has not been used yet.      *
 *                                                                            *
 ******************************************************************************/
int	zbx_dshm_validate_ref(const zbx_dshm_t *shm, zbx_dshm_ref_t *shm_ref, char **errmsg)
{
	if (shm->shmid == shm_ref->shmid && NULL != shm_ref->addr)
		return SUCCEED;

	if (NULL == (shm_ref->addr = shm_addr(shm->shmid)))
	{
		*errmsg = zbx_dsprintf(*errmsg, "cannot find shared memory segment %d", shm->shmid);
		return FAIL;
	}

	shm_ref->shmid = shm->shmid;

	return SUCCEED;
}

void	zbx_dshm_lock(zbx_dshm_t *shm)
{
	zbx_mutex_lock(shm->lock);
}

void	zbx_dshm_unlock(zbx_dshm_t *shm)
{
	zbx_mutex_unlock(shm->lock);
}
