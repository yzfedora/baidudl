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

#define DLINFO_REQ_SZ	4096
#define DLINFO_RCV_SZ	(1024 * 128)
#define DLINFO_SRV_SZ	64
#define DLINFO_HST_SZ	512
#define DLINFO_URL_SZ	4096
#define DLINFO_NAME_MAX	(NAME_MAX + 1)
#define DLINFO_ENCODE_NAME_MAX	(NAME_MAX * 3 + 1)

struct dlthreads {
	pthread_t		thread;
	struct dlpart		*dp;
	struct dlthreads	*next;
};

/* Used to packet following parameters into a structure. */
struct packet_args {
	struct dlinfo	*arg_dl;
	struct dlpart	**arg_dp;/* saved address of dp, used in download(). */
	ssize_t		arg_start;
	ssize_t		arg_end;
	int		arg_no;
};

#define PACKET_ARGS(pkt, dl, dp, start, end, no) 	\
	do {						\
		if ((pkt = malloc(sizeof(*(pkt))))) {	\
			(pkt)->arg_dl = dl;		\
			(pkt)->arg_dp = dp;		\
			(pkt)->arg_start = start;	\
			(pkt)->arg_end = end;		\
			(pkt)->arg_no = no;		\
		}					\
	} while (0)

#define UNPACKET_ARGS(pkt, dl, dp, start, end, no)	\
	do {						\
		dl = (pkt)->arg_dl;			\
		dp = (pkt)->arg_dp;			\
		start = (pkt)->arg_start;		\
		end = (pkt)->arg_end;			\
		no = (pkt)->arg_no;			\
	} while (0)

#define PACKET_ARGS_FREE(pkt)	do { free(pkt); } while (0)

struct dlinfo {
	int	di_remote;
	int	di_local;
	int	di_nthreads;	/* number of threads to download */
	ssize_t	di_length;	/* total length of the file */

	char	di_filename[DLINFO_NAME_MAX];
	char	di_serv[DLINFO_SRV_SZ];	/* service type */
	char	di_host[DLINFO_HST_SZ];	/* host name or IP address */
	char	di_url[DLINFO_URL_SZ];	/* original download url request */

	struct dlthreads *di_threads;
	
	int (*connect)(struct dlinfo *);/* used by dlpart_new */
	void (*setprompt)(struct dlinfo *);
	void (*rgstralrm)(void);
	void (*launch)(struct dlinfo *);
	void (*delete)(struct dlinfo *);
};

struct dlinfo *dlinfo_new(char *url, char *filename, int nthreads);
#endif
