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
#ifndef _DLINFO_H
#define _DLINFO_H
#include <sys/types.h>
#include <limits.h>
#include "dlpart.h"

#define DI_BUF_SZ		1024
#define DI_URL_SZ		4096
#define DI_NAME_MAX		(NAME_MAX + 1)
#define DI_ENC_NAME_MAX		(NAME_MAX * 3 + 1)
#define DI_PROMPT_SZ		1024
#define DI_PROMPT_RESERVED	45
#define DI_TRY_TIMES_MAX	7

struct dlthread {
	pthread_t		thread;
	struct dlpart		*dp;
	struct dlthread	*next;
};

/* Used to packet following parameters into a structure. */
struct args {
	struct dlinfo	*arg_dl;
	struct dlpart	**arg_dp;
	ssize_t		arg_start;
	ssize_t		arg_end;
	int		arg_no;
};

struct dlinfo {
	char	di_url[DI_URL_SZ];	/* original download url request */
	char	di_filename[DI_NAME_MAX];
	ssize_t	di_length;		/* total length of the file */
	int	di_local;		/* local file descriptor */

	int	di_nthreads;		/* number of threads to download */
	int	di_nthreads_curr;	/* current running threads */
	int	di_nthreads_total;

	char	di_prompt[DI_PROMPT_SZ];/* store the prompt string */
	char	di_file_size_string[16];/* file size in a string format */
	int	di_wincsz;		/* terminal widows columns */
	size_t	di_sig_cnt;		/* sigalrm counter */
	size_t	di_total;		/* length of file */
	size_t	di_total_read;		/* total read(received) bytes */
	size_t	di_bps;			/* bytes per second, speed */
	size_t	di_bps_last;		/* last second speed */


	int	di_url_is_http;		/* use to check HTTP or HTTPS code */
	int	di_recovery:1;		/* recovery from previous file */
	int	di_try_ignore_records:1;/* prevent restore from invalid file */

	pthread_mutex_t	di_mutex;

	struct dlthread		*di_threads;
	struct dlbuffer		*di_buffer;	/* used to cache header */

	void (*nthreads_inc)(struct dlinfo *);
	void (*nthreads_dec)(struct dlinfo *);
	void (*total_read_update)(struct dlinfo *, size_t);
	void (*bps_update)(struct dlinfo *, size_t);
	void (*bps_reset)(struct dlinfo *);
	int (*connect)(struct dlinfo *);/* used by dlpart_new */
	void (*setprompt)(struct dlinfo *);
	void (*rgstralrm)(void);
	void (*launch)(struct dlinfo *);
	void (*delete)(struct dlinfo *);
};

struct dlinfo *dlinfo_new(char *url, char *filename, int nthreads);
#endif
