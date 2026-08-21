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

/* Shared memory for the Windows build.
 *
 * On Windows every Zabbix worker is a thread of one process rather than a
 * forked child, so the segments that zbxnix/dshm.c places in System V shared
 * memory are plain heap allocations guarded by a critical section.
 *
 * Implements: zbx_shm_create, zbx_shm_destroy, zbx_dshm_create,
 * zbx_dshm_destroy, zbx_dshm_realloc, zbx_dshm_validate_ref, zbx_dshm_lock,
 * zbx_dshm_unlock.
 */

/* placeholder until the platform layer lands - keeps the translation unit
   from being empty, which MSVC reports as LNK4221 */
const int	zbx_win_shm_win_placeholder = 0;
