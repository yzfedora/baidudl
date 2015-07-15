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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "dlcommon.h"
#include "err_handler.h"


int retcode(char *s)
{
	char *p, *ep;
	int code;

	if (NULL != (p = strchr(s, ' '))) {
		p += 1;
		code = strtol(p, &ep, 10);
		return code;
	}
	return -1;
}

void nwrite(int fd, const void *buf, unsigned int len)
{
	unsigned int n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n == -1) {
			if (errno == EPIPE)
				err_exit(errno, "nwrite");
			err_msg(errno, "nwrite");
			continue;
		}
			
		len -= n;
		buf += n;
	}
}
