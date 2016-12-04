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
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "err_handler.h"
#include "dlcommon.h"
#include "dlinfo.h"


/* Get the return code. used to checking validity. */
int dlcom_get_http_response_code(char *s)
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

int dlcom_url_is_http(const char *url)
{
	if (!strncmp(url, "http://", 7) ||
	    !strncmp(url, "https://", 8) ||
	    !strstr(url, "://"))
		return 1;

	return 0;
}

void dlcom_get_filename_from_url(struct dlinfo *dl)
{
	char *p;
	char tmp[DI_ENC_NAME_MAX];

	if ((p = strrchr(dl->di_url, '/')))
		strncpy(tmp, p + 1, sizeof(tmp) - 1);
	else
		strncpy(tmp, dl->di_url, sizeof(tmp) - 1);

	tmp[DI_ENC_NAME_MAX - 1] = 0;
	strncpy(dl->di_filename, dlcom_string_decode(tmp),
		sizeof(dl->di_filename) - 1);
}

int dlcom_get_terminal_width(void)
{
	struct winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1)
		err_sys("ioctl get the window size");
	return size.ws_col;
}

ssize_t dlcom_writen(int fd, const void *buf, size_t count)
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
			err_exit("dlcom_writen");
		}
	}
	return count;
}

/*
 * Used to decode the filename string ''
 */
char *dlcom_string_decode(char *src)
{
	char tmp[DI_ENC_NAME_MAX];
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

char *dlcom_strcasestr(const char *haystack, const char *needle)
{
	size_t i;
	size_t haystack_buflen = strlen(haystack) + 1;
	size_t needle_buflen = strlen(needle) + 1;
	char *haystack_buf = alloca(haystack_buflen);
	char *needle_buf = alloca(needle_buflen);
	char *needle_ptr;

	if (!haystack_buf || !needle_buf)
		return NULL;

	for (i = 0; i < haystack_buflen; i++)
		haystack_buf[i] = tolower((int)haystack[i]);

	for (i = 0; i < needle_buflen; i++)
		needle_buf[i] = tolower((int)needle[i]);

	if (!(needle_ptr = strstr(haystack_buf, needle_buf)))
		return NULL;

	return (char *)haystack + (needle_ptr - haystack_buf);
}

int dlcom_http_response_code_is_valid(struct dlinfo *dl, int rc)
{
	if (dl->di_url_is_http) {
		if ((dl->di_nthreads == 1 && rc != 200 && rc != 206) ||
		    (dl->di_nthreads > 1 && rc != 206))
		return 0;
	}

	return 1;
}
