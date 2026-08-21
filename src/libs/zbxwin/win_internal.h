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

#ifndef ZABBIX_WIN_INTERNAL_H
#define ZABBIX_WIN_INTERNAL_H

#include "zbxnix.h"

/* counterpart of src/libs/zbxnix/nix_internal.h: state registered through the
   public interface and shared between the translation units of this library */

zbx_get_progname_f			win_get_progname_cb(void);
zbx_get_process_info_by_thread_f	win_get_process_info_by_thread_func_cb(void);

ZBX_THREAD_HANDLE	*win_get_child_threads(size_t *num);
void			win_call_on_exit(int ret);

#endif /* ZABBIX_WIN_INTERNAL_H */
