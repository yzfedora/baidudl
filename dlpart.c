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
#define _POSIX_C_SOURCE	200809L
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include "dlpart.h"
#include "dlcommon.h"
#include "err_handler.h"

/* HTTP Header format:
 * 	GET url\r\n HTTP/1.1\r\n
 * 	Range: bytes=x-y
 */
void dlpart_send_header(struct dlpart *dp)
{
	char sbuf[DLPART_BUFSZ];

	sprintf(sbuf,	"GET %s HTTP/1.1\n"
			"Host: %s\n"
			"Range: bytes=%ld-%ld\r\n\r\n",
			dp->dp_info->di_url, dp->dp_info->di_host,
			(long)dp->dp_start, (long)dp->dp_end);

#ifdef __DEBUG__
	printf("---------------Sending Header(%ld-%ld)----------------\n"
		"%s---------------------------------------------------\n",
		dp->dp_start, dp->dp_end, sbuf);
#endif
	nwrite(dp->dp_remote, sbuf, strlen(sbuf));
}

int dlpart_recv_header(struct dlpart *dp)
{
	int is_header = 1, code;
	ssize_t start = -1, end = -1;
	char *dt, *p, *ep;

	while (is_header) {
		dp->read(dp, NULL, NULL);
		if (dp->dp_nrd <= 0)
			return -1;

		if ((dt = strstr(dp->dp_buf, "\r\n\r\n")) != NULL) {
			*dt = 0;
			dt += 4;
			dp->dp_nrd -= (strlen(dp->dp_buf) + 4);
			is_header = 0;
		}
#ifdef __DEBUG__
		printf("------------Receiving Heading(%ld-%ld)----------\n"
				"%s\n-----------------------------------\n",
				dp->dp_start, dp->dp_end, dp->dp_buf);
#endif
		/* Range request should return 206 code */
		if ((code = retcode(dp->dp_buf)) != 206)
			return -1;

#define RANGE	"Content-Range: bytes "
		if ((p = strstr(dp->dp_buf, RANGE)) != NULL) {
			start = strtol(p + sizeof(RANGE) - 1, &ep, 10);
			end = (ep && *ep) ? strtol(ep + 1, NULL, 10) : end;
			/*if  (ep && *ep == '-')
				end = strtol(ep + 1, NULL, 10);*/
		}
	}
	
	if (start < 0 || end < start)
		return -1;
		/*err_exit(0, "Invalid range: bytes=%ld-%ld", start, end);*/

	dp->dp_start = start;
	dp->dp_end = end;
	/* FUCKING THE IMPLEMENTATION OF STRNCPY! try using memcpy() instead.
	 * strncpy(dp->dp_buf, dt, dp->dp_nrd);
	 */
	memmove(dp->dp_buf, dt, dp->dp_nrd);
	return 0;
}

void dlpart_read(struct dlpart *dp, ssize_t *total_read,
		ssize_t * bytes_per_sec)
{
	/* 
	 * If any errors occured, -1 will returned, and errno will be
	 * set correctly
	 */
	dp->dp_nrd = read(dp->dp_remote, dp->dp_buf, DLPART_BUFSZ - 1);
	if (dp->dp_nrd <= 0)
		return;

	/* 
	 * Since we updated the 'total_read' and 'bytes_per_sec', calling
	 * this dlpart_read need to be protected by pthead_mutex
	 */
	dp->dp_buf[dp->dp_nrd] = 0;
	if (total_read)
		*total_read += dp->dp_nrd;
	if (bytes_per_sec)
		*bytes_per_sec += dp->dp_nrd;
}

/*
 * Write 'dp->dp_nrd' data which pointed by 'dp->dp_buf' to local file.
 * And the offset 'dp->dp_start' will be updated also.
 */
void dlpart_write(struct dlpart *dp)
{
	unsigned int n, len = dp->dp_nrd;
	char *buf = dp->dp_buf;

	while (len > 0) {
		/*debug("pwrite %d bytes >> fd(%d), offset %ld\n",
			len, fd, offset);*/
		n = pwrite(dp->dp_local, buf, len, dp->dp_start);
		if (n == -1) {
			err_msg(errno, "pwrite %d", dp->dp_local);
			continue;
		}
		
		len -= n;
		buf += n;	
		dp->dp_start += n;
	}

}

void dlpart_delete(struct dlpart *dp)
{
	if (close(dp->dp_remote) == -1)
		err_msg(errno, "close %d", dp->dp_remote);
	free(dp);
}

struct dlpart *dlpart_new(struct dlinfo *dl, ssize_t start, ssize_t end)
{
	struct dlpart *dp;

	if ((dp = (struct dlpart *)malloc(sizeof(*dp))) == NULL)
		return NULL;

	memset(dp, 0, sizeof(*dp));
	dp->dp_remote = dl->connect(dl);
	/* copy basic info from 'struct dlinfo' */
	dp->dp_local = dl->di_local;
	dp->dp_info = dl;

	dp->sendhdr = dlpart_send_header;
	dp->recvhdr = dlpart_recv_header;
	dp->read = dlpart_read;
	dp->write = dlpart_write;
	dp->delete = dlpart_delete;

try_sendhdr_again:
	dp->dp_start = start;
	dp->dp_end = end;

	dp->sendhdr(dp);
	if (dp->recvhdr(dp) == -1) {
		if (close(dp->dp_remote) == -1)
			err_msg(errno, "close");

		dp->dp_remote = dl->connect(dl);
		goto try_sendhdr_again;
	}

	/*
	 * for to get maximun of concurrency, set dp->dp_remote to nonblock.
	 */
	int flags;
	if ((flags = fcntl(dp->dp_remote, F_GETFL, 0)) == -1)
		err_exit(errno, "fcntl-getfl");
	flags |= O_NONBLOCK;
	if (fcntl(dp->dp_remote, F_SETFL, flags) == -1)
		err_exit(errno, "fcntl-setfl");

	return dp;
}
