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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include "err_handler.h"

void _err_exit(int flags, int err, char *fmt, ...)
{
#undef	EMSG_BUF
#define	EMSG_BUF	1024
	va_list ap;
	char buf[EMSG_BUF];

	va_start(ap, fmt);
	*buf = 0;
	strcat(buf, fmt);
	/* If errno is not zero, print the suitable string for errno */
	if (err != 0) {
		if (fmt != NULL && *fmt)
			strcat(buf, ": ");
		strcat(buf, strerror(err));
	}

	strcat(buf, "\n");

	vfprintf(stderr, buf, ap);
	va_end(ap);

	if (flags & ERR_EXIT)
		exit(EXIT_FAILURE);
	else if (flags & ERR_THREAD_EXIT)
		pthread_exit((void *)EXIT_FAILURE);
}
