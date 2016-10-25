/* This is a multi-thread download tool, for pan.baidu.com
 *   		Copyright (C) 2015  Yang Zhang <yzfedora@gmail.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/
#ifndef _DLBUFFER_H
#define _DLBUFFER_H
#include <stdio.h>

/*
 * Note: it's just save all data in a single memory area, so if you want to
 * store mass of data, more than 200MB or 2GB, it may failed.
 *
 * these API is used to replace open_memstream, because it's not implement on
 * OSX yet.
 */
#define DLBUFFER_INCREASE_SIZE	(1 << 18)	/* 256 KiB */

struct dlbuffer {
	char	*buf;
	size_t	pos;
	size_t	len;
};

int dlbuffer_write(struct dlbuffer *db, void *buf, size_t size);
void dlbuffer_delete(struct dlbuffer *db);
struct dlbuffer *dlbuffer_new(void);
#endif
