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
#include <err_handler.h>
#include <errno.h>
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

/* return a pointer to start of URI on success, or no any URI can be find,
 * NULL will be returned. */
char *geturi(const char *s, const char *u)
{
	char *p = strstr(s, u);

	if (!p)
		return NULL;

	p += strlen(u);
	return p;
}

void nwrite(int fd, const void *buf, unsigned int len)
{
	unsigned int n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n == -1) {
			if (errno == EPIPE)
				err_exit("nwrite");
			err_sys("nwrite");
			continue;
		}
			
		len -= n;
		buf += n;
	}
}

/*
 * Used to decode the filename string ''
 */
char *string_decode(char *src)
{
#define ENCODE_NAME_MAX	(NAME_MAX * 3 + 1)
	char tmp[ENCODE_NAME_MAX], *s = src;
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
