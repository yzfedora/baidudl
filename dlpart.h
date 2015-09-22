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
#ifndef _DLPART_H
#define _DLPART_H
#include <sys/types.h>
#include "dlinfo.h"

#define DLPART_BUFSZ	(1024 * 1024)

struct dlpart {
	int	dp_no;
	int	dp_remote;	/* remote server file descriptor */
	ssize_t	dp_start;
	ssize_t	dp_end;
	char	dp_buf[DLPART_BUFSZ];
	int	dp_nrd;
	struct dlinfo *dp_info;

	void (*sendhdr)(struct dlpart *);
	int  (*recvhdr)(struct dlpart *);
	void (*read)(struct dlpart *);
	void (*write)(struct dlpart *, ssize_t *, ssize_t *);
	void (*delete)(struct dlpart *);
};

struct dlpart *dlpart_new(struct dlinfo *, ssize_t, ssize_t, int);
#endif
