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
#include <sys/ioctl.h>

#include "err_handler.h"
#include "dlcommon.h"
#include "dlinfo.h"


/* Get the return code. used to checking validity. */
int getrcode(char *s)
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

int getwcol(void)
{
	struct winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1)
		err_sys("ioctl get the window size");
	return size.ws_col;
}

ssize_t writen(int fd, const void *buf, size_t count)
{
	int nwrt;
	const char *ptr = buf;
	size_t nleft = count;

	errno = 0;
	while (nleft > 0) {
		if ((nwrt = write(fd, ptr, nleft)) > 0) {
			nleft -= nwrt;
			ptr += nwrt;
		} else if (nwrt == 0) {
			return 0;
		} else {
			if (errno == EINTR)
				continue;
			err_exit("writen");
		}
	}
	return count;
}

/*
 * Used to decode the filename string ''
 */
char *string_decode(char *src)
{
	char tmp[DLINFO_ENCODE_NAME_MAX];
	char *s = src;
	int i, j, k, t;

	/* strip the double-quotes */
	for (i = j = k = 0; s[i] && s[i] != '\r' && s[i+1] != '\n'; i++) {
		if (s[i] != '"')
			tmp[j++] = s[i];
		else {
			if (2 == ++k)
				break;
		}
	}
	tmp[j] = 0;

	s = src;
	for (i = j = k = 0; tmp[i]; i++, j++) {
		if (tmp[i] == '%') {
			/* following 2 bytes is hex-decimal of a char */
			t = tmp[i + 3];
			tmp[i + 3] = 0;

			k = strtol(tmp + i + 1, NULL, 16);
			s[j] = (char)k;

			tmp[i + 3] = t;
			i += 2;
		} else {
			s[j] = tmp[i];
		}
	}
	s[j] = 0;

	return s;
}
