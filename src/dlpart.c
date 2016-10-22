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
#define _DEFAULT_SOURCE
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

#if (defined(__APPLE__) && defined(__MACH__))
# include <malloc/malloc.h>
#endif

#include "err_handler.h"
#include "dlpart.h"
#include "dlcommon.h"


/*
 * Continuous download supports:
 * | total_length | nthreads | range 1 | ... | range n |
 * | 8 bytes      | 4 bytes  | 8 x 2   |     | 8 x 2   |
 */
static void dlpart_update(struct dlpart *dp)
{
	struct dlinfo *dl = dp->dp_info;
	int ret;
	int local = dl->di_local;
	typeof(dp->dp_start) start = dp->dp_start;
	typeof(dp->dp_end) end = dp->dp_end;
	ssize_t offset = dl->di_length + sizeof(dl->di_nthreads) +
			sizeof(start) * dp->dp_no * 2;

try_write_start_again:
	ret = pwrite(local, &start, sizeof(start), offset);
	if (ret != sizeof(start))
		goto try_write_start_again;

try_write_end_again:
	ret = pwrite(local, &end, sizeof(end), offset + sizeof(start));
	if (ret != sizeof(end))
		goto try_write_end_again;
}

/*
 * Write 'dp->dp_nrd' data which pointed by 'dp->dp_buf' to local file.
 * And the offset 'dp->dp_start' will be updated also.
 */
static void dlpart_write(struct dlpart *dp)
{
	int n, len = dp->dp_nrd;
	char *buf = dp->dp_buf;
	struct dlinfo *dl = dp->dp_info;

	while (len > 0) {
		n = pwrite(dp->dp_info->di_local, buf, len, dp->dp_start);
		if (n > 0) {
			len -= n;
			buf += n;
			dp->dp_start += n;
		} else if (n == 0) {
			err_exit("pwrite end-of-file");
		} else {
			if (errno == EINTR)
				continue;
			err_sys("pwrite");
		}
	}

	dl->total_read_update(dl, dp->dp_nrd);
	dl->bps_update(dl, dp->dp_nrd);

	dlpart_update(dp);
}

static size_t dlpart_write_callback(char *buf,
				    size_t size,
				    size_t nitems,
				    void *userdata)
{
	struct dlpart *dp = (struct dlpart *)userdata;

	dp->dp_nrd = size * nitems;
	dp->dp_buf = buf;

	dlpart_write(dp);
	return dp->dp_nrd;
}

static int dlpart_curl_setup(struct dlpart *dp)
{
	char range[DLPART_BUFSZ];

	snprintf(range, sizeof(range), "%ld-%ld", dp->dp_start, dp->dp_end);
	curl_easy_setopt(dp->dp_curl, CURLOPT_RANGE, range);
	curl_easy_setopt(dp->dp_curl, CURLOPT_URL, dp->dp_info->di_url);
	curl_easy_setopt(dp->dp_curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dp->dp_curl, CURLOPT_WRITEFUNCTION,
			 dlpart_write_callback);
	curl_easy_setopt(dp->dp_curl, CURLOPT_WRITEDATA, dp);
	curl_easy_setopt(dp->dp_curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(dp->dp_curl, CURLOPT_SSL_VERIFYPEER, 0);
	return 0;
}

static int dlpart_launch(struct dlpart *dp)
{
	CURLcode rc;

	dlpart_curl_setup(dp);
	if ((rc = curl_easy_perform(dp->dp_curl))) {
		err_msg("curl_easy_perform: %s", curl_easy_strerror(rc));
		return -1;
	}

	return 0;
}

static void dlpart_delete(struct dlpart *dp)
{
	if (!dp)
		return;

	if (dp->dp_curl)
		curl_easy_cleanup(dp->dp_curl);

	free(dp);
}

struct dlpart *dlpart_new(struct dlinfo *dl, ssize_t start, ssize_t end, int no)
{
	struct dlpart *dp;

	if (!(dp = (struct dlpart *)malloc(sizeof(*dp))))
		return NULL;

	memset(dp, 0, sizeof(*dp));
	if (!(dp->dp_curl = curl_easy_init())) {
		err_msg("curl_easy_init");
		dlpart_delete(dp);
		return NULL;
	}

	dp->dp_no = no;
	dp->dp_info = dl;

	dp->launch = dlpart_launch;
	dp->delete = dlpart_delete;

	dp->dp_start = start;
	dp->dp_end = end;
	dlpart_update(dp);	/* Force to write the record into end of file*/

	return dp;
}
