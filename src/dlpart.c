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
#include "dlbuffer.h"


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

static void dlpart_write(struct dlpart *dp)
{
	int n, len = dlbuffer_get_offset(dp->dp_buf);
	char *buf = dlbuffer_get_buffer(dp->dp_buf);

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


	dlpart_update(dp);
}

static size_t dlpart_http_header_callback(char *buf,
					  size_t size,
					  size_t nitems,
					  void *userdata)
{
	size_t len = size * nitems;
	struct dlpart *dp = (struct dlpart *)userdata;

	if (dlbuffer_write(dp->dp_buf, buf, len) < 0)
		return 0;

	if (!strcmp(buf, "\r\n")) {
		dlbuffer_get_buffer(dp->dp_buf)[dlbuffer_get_offset(dp->dp_buf)] = 0;
		int rc = dlcom_get_http_response_code(dlbuffer_get_buffer(dp->dp_buf));
		if (!dlcom_http_response_code_is_valid(dp->dp_info, rc)) {
			err_dbg(2, "remote server may not supports multithreading download");
			dp->dp_ready = 0;
		}

		dlbuffer_set_offset(dp->dp_buf, 0);
	}

	return len;
}

static size_t dlpart_write_callback(char *buf,
				    size_t size,
				    size_t nitems,
				    void *userdata)
{
	struct dlpart *dp = (struct dlpart *)userdata;
	struct dlinfo *dl = dp->dp_info;
	size_t len = size *nitems;

	if (!dp->dp_ready)
		return 0;

	if (dlbuffer_write(dp->dp_buf, buf, len) < 0)
		return 0;

	dl->total_read_update(dl, len);
	dl->bps_update(dl, len);

	/*
	 * to make the IO as fast as possible, we use dlbuffer APIs to cache
	 * the data, and write to disk when cached more than 1MiB bytes.
	 */
	if (dlbuffer_get_offset(dp->dp_buf) > DLPART_CACHE_SIZE) {
		dlpart_write(dp);
		dlbuffer_set_offset(dp->dp_buf, 0);
	}

	return len;
}

static int dlpart_curl_setup(struct dlpart *dp)
{
	char range[DLPART_BUFSZ];

	snprintf(range, sizeof(range), "%ld-%ld", dp->dp_start, dp->dp_end);
	curl_easy_setopt(dp->dp_curl, CURLOPT_RANGE, range);
	curl_easy_setopt(dp->dp_curl, CURLOPT_URL, dp->dp_info->di_url);
	curl_easy_setopt(dp->dp_curl, CURLOPT_FOLLOWLOCATION, 1);

	/* do HTTP response code check if the url is HTTP or HTTPS. */
	if (dp->dp_info->di_url_is_http) {
		curl_easy_setopt(dp->dp_curl, CURLOPT_HEADERFUNCTION,
				 dlpart_http_header_callback);
		curl_easy_setopt(dp->dp_curl, CURLOPT_HEADERDATA, dp);
	}

	curl_easy_setopt(dp->dp_curl, CURLOPT_WRITEFUNCTION,
			 dlpart_write_callback);
	curl_easy_setopt(dp->dp_curl, CURLOPT_WRITEDATA, dp);
	curl_easy_setopt(dp->dp_curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(dp->dp_curl, CURLOPT_SSL_VERIFYPEER, 0);
	return 0;
}

static int dlpart_launch(struct dlpart *dp)
{
	int ret = -1;
	CURLcode rc;

	dlpart_curl_setup(dp);
	if ((rc = curl_easy_perform(dp->dp_curl)) && rc != CURLE_WRITE_ERROR) {
		err_dbg(2, "curl_easy_perform: %s", curl_easy_strerror(rc));
		goto out;
	}

	ret = 0;
out:
	dlpart_write(dp);
	return ret;
}

static void dlpart_delete(struct dlpart *dp)
{
	if (!dp)
		return;

	if (dp->dp_curl)
		curl_easy_cleanup(dp->dp_curl);
	if (dp->dp_buf)
		dlbuffer_delete(dp->dp_buf);
	free(dp);
}

struct dlpart *dlpart_new(struct dlinfo *dl, ssize_t start, ssize_t end, int no)
{
	struct dlpart *dp = NULL;

	if (!(dp = (struct dlpart *)malloc(sizeof(*dp))))
		goto out;

	memset(dp, 0, sizeof(*dp));
	if (!(dp->dp_curl = curl_easy_init())) {
		err_msg("curl_easy_init");
		goto out;
	}

	if (!(dp->dp_buf = dlbuffer_new())) {
		err_sys("dlbuffer_new");
		goto out;
	}

	dp->dp_ready = 1;
	dp->dp_no = no;
	dp->dp_info = dl;

	dp->launch = dlpart_launch;
	dp->delete = dlpart_delete;

	dp->dp_start = start;
	dp->dp_end = end;
	dlpart_update(dp);	/* Force to write the record into end of file*/

	return dp;
out:
	dlpart_delete(dp);
	return NULL;
}
