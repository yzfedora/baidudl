#define _POSIX_C_SOURCE	200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
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
	/* If any errors occured, the dp->dp_nrd will be set accordingly */
	dp->dp_nrd = read(dp->dp_remote, dp->dp_buf, DLPART_BUFSZ - 1);
	if (dp->dp_nrd <= 0)
		return;

	/*printf("\nthread %ld read %d bytes.\n", (long)pthread_self(), dp->dp_nrd);*/
	dp->dp_buf[dp->dp_nrd] = 0;
	/* Since we updated the 'total_read' and 'bytes_per_sec', calling
	 * this dlpart_read need to be protected by pthead_mutex */
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
			err_msg(errno, "pwrite");
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

struct dlpart *dlpart_new(struct dlinfo *dl)
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

	return dp;
}
