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
#include <stdlib.h>		/* strtol() */
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <err_handler.h>

#include "dlinfo.h"
#include "dlpart.h"
#include "dlcommon.h"
#include "scrolling_display.h"

/*
 * Protect the variables 'threads_total' and 'threads_curr' in multi-threads.
 */
static pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;
#define MEMORY_LOCK()							\
	do {								\
		int rc;							\
		if ((rc = pthread_mutex_lock(&memory_lock)) != 0)	\
			err_sys("memory lock");				\
	} while (0)
#define MEMORY_UNLOCK()							\
	do {								\
		int rc;							\
		if ((rc = pthread_mutex_unlock(&memory_lock)) != 0)	\
			err_sys("memory unlock");			\
	} while (0)
#define SAFE_OPERATION(statement)					\
	MEMORY_LOCK();							\
	statement;							\
	MEMORY_UNLOCK();

/* 
 * Using to print the progress, percent... of download info.
 */
#define DLINFO_PROMPT_SZ	1024
#define DLINFO_TRYTIMES_MAX	30
#define DLINFO_NEW_TRYTIMES_MAX	3
/* Reserved 50 column for others prompt */
#define DLINFO_PROMPT_RESERVED	48

static int dorecovery;
static int try_ignore_records;
static int threads_curr;		/* Number of current threads doing download() */
static int threads_total;
static char prompt[DLINFO_PROMPT_SZ];
static char file_size_str[16];	/* File size string. */
static ssize_t total, total_read, bytes_per_sec;
static long sig_cnt;
static unsigned int winsize_column;

static void *download(void *arg);


/*
 * Initial the service and host of the server. parsed url exclude the trailing
 * '/' character.
 */
static char *dlinfo_init(struct dlinfo *dl)
{
	char *start;
	char *ret = dl->di_host;
	char *url = dl->di_url;
	char *slash, *ptr;
	char saved;

	if (!(start = strstr(url, "://"))) {
		start = url;
	} else {		/* parsing service name */
		start += 3;
	}


	if ((slash = strchr(start, '/'))) {
		saved = *slash;
		*slash = 0;
		snprintf(dl->di_uri, DLINFO_URI_SZ, "/%s", slash + 1);
	} else {
		strncpy(dl->di_uri, "/", DLINFO_URI_SZ);
	}

	if ((ptr = strchr(start, ':'))) {
		char c = *ptr;
		*ptr = 0;
		strncpy(dl->di_host, start, DLINFO_HST_SZ);

		strncpy(dl->di_serv, ptr + 1, DLINFO_SRV_SZ);
		*ptr = c;
	} else {
		strncpy(dl->di_host, start, DLINFO_HST_SZ);
	}


	if (slash)
		*slash = saved;

	return ret;
}

/*
 * Store the file descriptor to the '*sockfd' which is connected to
 * server already. and return the socket file descriptor also.
 */
int dlinfo_connect(struct dlinfo *dl)
{
	int s;
	int fd;
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	errno = 0;
	if ((s = getaddrinfo(dl->di_host, dl->di_serv, &hints, &res)) != 0)
		err_exit("getaddrinfo: %s, host: %s:%s", gai_strerror(s),
			 dl->di_host, dl->di_serv);

	for (ai = res; ai; ai = ai->ai_next) {
		if ((fd = socket(ai->ai_family, ai->ai_socktype,
				 ai->ai_protocol)) == -1) {
			err_sys("socket");
			continue;
		}

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;

		err_sys("connect");
		if (close(fd) == -1)
			err_sys("close");
	}

	if (NULL == ai)
		err_exit("Couldn't connection to: %s", dl->di_host);

	freeaddrinfo(res);
	return fd;
}

/*
 * Send HTTP HEAD request to server for retriving the length and filename
 * of the file.
 */
static void dlinfo_send_request(struct dlinfo *dl)
{
	char buf[DLINFO_REQ_SZ];

	/* not using HEAD request, sometime HEAD will produce 400 error. */
	sprintf(buf, "GET %s HTTP/1.1\r\n"
		"Host: %s\r\n\r\n", dl->di_uri, dl->di_host);
	writen(dl->di_remote, buf, strlen(buf));
	err_dbg(2, "-------------- Send Requst (Meta info) -----------\n"
			"%s", buf);
}

/*
 * Receive the response from the server, which include the HEAD info. And
 * store result to dl->di_length, dl->di_filename.
 */
static int dlinfo_recv_and_parsing(struct dlinfo *dl)
{
	int n;
	int code;
	char buf[DLINFO_RCV_SZ];
	char tmp[DLINFO_ENCODE_NAME_MAX];
	char *p;

	/* any error or end-of-file will cause parsing header fails */
	if ((n = read(dl->di_remote, buf, DLINFO_RCV_SZ - 1)) <= 0)
		return -1;

	buf[n] = 0;
	err_dbg(2, "--------------Received Meta info---------------\n"
		"%s\n", buf);

	/* response code valid range [200-300) */
	code = getrcode(buf);
	if (code < 200 || code >= 300)
		return -1;

#define _CONTENT_LENGTH	"Content-Length: "
	if (NULL != (p = strstr(buf, _CONTENT_LENGTH))) {
		dl->di_length = strtol(p + sizeof(_CONTENT_LENGTH) - 1,
				       NULL, 10);
	}

	/* User specified filename */
	if (dl->di_filename && *dl->di_filename)
		return 0;

#define FILENAME	"filename="
	if ((p = strstr(buf, FILENAME))) {
		p = memccpy(tmp, p + sizeof(FILENAME) - 1, '\n',
				DLINFO_ENCODE_NAME_MAX);
		if (!p) {
			err_sys("memccpy");
			return -1;
		}
		strncpy(dl->di_filename, string_decode(tmp),
			sizeof(dl->di_filename) - 1);
		return 0;
	}

	/* if filename parsing failed, then parsing filename from url. */
	if ((p = strrchr(dl->di_url, '/'))) {
		strcpy(tmp, p + 1);
		strncpy(dl->di_filename, string_decode(tmp),
			sizeof(dl->di_filename) - 1);
		return 0;
	}
	return -1;
}

/*
 * dlinfo_records_* functions is NOT Multi-Threads Safe, because it using
 * file's offset to read data. any more than one threads read from it at
 * same time will cause unknown results.
 */
static void dlinfo_records_recovery(struct dlinfo *dl, void *buf,
				    ssize_t len)
{
	if (read(dl->di_local, buf, len) != len)
		err_exit("records recovery occur errors");
}

static int dlinfo_download_is_finished(struct dlinfo *dl)
{
	ssize_t real = lseek(dl->di_local, 0, SEEK_END);
	return (real == dl->di_length) ? 1 : 0;
}

static int dlinfo_records_recovery_nthreads(struct dlinfo *dl)
{
	ssize_t filelen = lseek(dl->di_local, 0, SEEK_END);
	ssize_t recordlen;
	int save_nthreads = dl->di_nthreads;

	/* seek to the start of the records. and retriving number of threads. */
	lseek(dl->di_local, dl->di_length, SEEK_SET);
	dlinfo_records_recovery(dl, &dl->di_nthreads,
				sizeof(dl->di_nthreads));

	/* calculate the records length. it should be equal the ('filelen' -
	 * dl->di_length) */
	recordlen = dl->di_nthreads * 2 * sizeof(dl->di_threads->dp->dp_start)
		+ sizeof(dl->di_nthreads);


	/* unrecognized record format, setting try_ignore_records flags */
	if (dl->di_length + recordlen != filelen) {
		dl->di_nthreads = save_nthreads;
		try_ignore_records = 1;
		return -1;
	}
	return 0;
}

/*
 * Recovery the 'total_read' field, and the range of per threads should
 * download where start from.
 */
static ssize_t dlinfo_records_recovery_all(struct dlinfo *dl)
{
	int i, s;
	ssize_t start;
	ssize_t end;
	ssize_t nedl = 0;
	struct dlthreads **dt = &dl->di_threads;

	if (dlinfo_records_recovery_nthreads(dl) == -1)
		return -1;

	/* Initial the number of all threads */
	threads_total = dl->di_nthreads;

	/* this isn't necessary, but for a non-dependencies impl. */
	lseek(dl->di_local, dl->di_length + sizeof(dl->di_nthreads), SEEK_SET);
	for (i = 0; i < dl->di_nthreads; i++) {
		struct packet_args *pkt;
		/*
		 * if dt is second pointer in the linked list, dt = dt->next.
		 * we need to malloc a block memory for it.
		 */
		if (!*dt) {
			if (!(*dt = malloc(sizeof(**dt))))
				err_exit("malloc");
			memset(*dt, 0, sizeof(**dt));
		}

		dlinfo_records_recovery(dl, &start, sizeof(start));
		dlinfo_records_recovery(dl, &end, sizeof(end));
		if (start > end) {
			/* 
			 * if this range is finished, set 'dt->thread' and
			 * 'dt->dp both to 0 or NULL properly'.
			 */
			if (start != end + 1)
				err_exit("recovery error range: %ld-%ld\n",
					    start, end);
			/* 
			 * Set members of *dt to 0 or NULL. subtract 1 for
			 * finished part.
			 */
			(*dt)->thread = 0;
			(*dt)->dp = NULL;
			SAFE_OPERATION(threads_total--;)
			goto next_range;
		}

		PACKET_ARGS(pkt, dl, &(*dt)->dp, start, end, i);
		if (!pkt)
			err_exit("packet arguments");

		if ((s = pthread_create(&(*dt)->thread, NULL,
					download, pkt)) != 0) {
			errno = s;
			err_exit("pthread_create");
		}
		
		nedl += (end - start) + 1;
next_range:
		dt = &((*dt)->next);
	}

	total_read += total - nedl;
	return total_read;
}


static void dlinfo_records_removing(struct dlinfo *dl)
{
	if (ftruncate(dl->di_local, dl->di_length) == -1)
		err_exit("ftruncate");
}

/*
 * Open a file descriptor for store the download data, and store the
 * file descriptor to variable dl->di_local.
 */
static int dlinfo_open_local_file(struct dlinfo *dl)
{
#define PERMS (S_IRUSR | S_IWUSR)
	int fd;
	int ret;
	int flags = O_RDWR | O_CREAT | O_EXCL;

	if ((fd = open(dl->di_filename, flags, PERMS)) == -1) {
		if (errno == EEXIST) {
			/*
			 * same file already exists. try recovery records
			 */
			flags &= ~O_EXCL;
			if ((fd = open(dl->di_filename, flags, PERMS)) == -1)
				err_exit("open");

			dorecovery = 1;
			goto dlinfo_open_local_file_return;
		}
		err_exit("open");
	}

	/* If no file exists, append number of threads records to the file. */
try_pwrite_nthreads_again:
	ret = pwrite(fd, &dl->di_nthreads,
		     sizeof(dl->di_nthreads), dl->di_length);
	if (ret != sizeof(dl->di_nthreads))
		goto try_pwrite_nthreads_again;

dlinfo_open_local_file_return:
	dl->di_local = fd;
	return fd;
}


/**********************************************************************
 *                     basic download implementations                 *
 **********************************************************************/

/*
 * Construct a fixed prompt, like: "download foo.zip, size 128MB    16%".
 * and return a pointer to the prompt string.
 */
static char *dlinfo_set_prompt(struct dlinfo *dl)
{
	short flags = 0;
	ssize_t size;
	ssize_t orig_size;

	orig_size = size = total;
	while (size > 1024) {
		size >>= 10;
		flags++;
	}

	snprintf(prompt, sizeof(prompt), "\e[7mDownload: ");
	snprintf(file_size_str, sizeof(file_size_str), "%6.1f%-5s",
		 orig_size / ((double) (1 << (10 * flags))),
		 (flags == 0) ? "Bytes" :
		 (flags == 1) ? "KB" :
		 (flags == 2) ? "MB" :
		 (flags == 3) ? "GB" : "TB");

	sig_cnt = 0;

	winsize_column = getwcol();	/* initialize window column. */

	/* 
	 * Initial roll displayed string, and expect maximum length.
	 */
	if (scrolling_display_init(dl->di_filename, winsize_column -
				DLINFO_PROMPT_RESERVED) == -1)
		err_exit("Setting roll display function error");
	return prompt;
}

/* if dl->di_filename is more than 30 bytes, then dynamically print the
 * full name of this file. */
static void dlinfo_set_prompt_dyn(void)
{
	unsigned int len, padding;
	char *ptr = scrolling_display_ptr(&len, &padding);

	snprintf(prompt + 14, sizeof(prompt) - 14, "%.*s  %*s%s", len, ptr,
		 padding, "", file_size_str);
	sig_cnt++;
}

/*
 * To prevent the round up of snprintf().
 * example:
 * 	99.97 may be round up to 100.0
 */
static char *dlinfo_get_percentage(void)
{
#define DLINFO_PERCENTAGE_STR_MAX	32
	int len;
	static char percentage_str[DLINFO_PERCENTAGE_STR_MAX];
	len = snprintf(percentage_str, sizeof(percentage_str), "%6.2f",
			(double)total_read * 100 / total);
	percentage_str[len - 1] = 0;
	return percentage_str;
}

static void dlinfo_sigalrm_handler(int signo)
{
	ssize_t speed = bytes_per_sec;

	speed >>= 10;

	dlinfo_set_prompt_dyn();
	printf("\r" "%*s", winsize_column, "");
	printf("\r%s %4ld%s/s  %s%% \e[31m[%2d/%-2d]\e[0m", prompt,
	       (long)speed,
	       "KB",
	       dlinfo_get_percentage(),
	       threads_curr, threads_total);

	fflush(stdout);
	bytes_per_sec = 0;
}

static void dlinfo_sigwinch_handler(int signo)
{
	winsize_column = getwcol();
	scrolling_display_setsize(winsize_column - DLINFO_PROMPT_RESERVED);
}

static void dlinfo_register_signal_handler(void)
{
	struct sigaction act, old;

	memset(&act, 0, sizeof(act));
	act.sa_flags |= SA_RESTART;
	act.sa_handler = dlinfo_sigalrm_handler;

	/* 
	 * Register the SIGALRM handler, for print the progress of download.
	 */
	if (sigaction(SIGALRM, &act, &old) == -1)
		err_exit("sigaction - SIGALRM");

	/*
	 * Register the SIGWINCH signal, for adjust the output length of
	 * progress when user change the window size.
	 */
	act.sa_handler = dlinfo_sigwinch_handler;
	if (sigaction(SIGWINCH, &act, &old) == -1)
		err_exit("sigaction - SIGWINCH");
}


static void dlinfo_sigalrm_detach(void)
{
	if (setitimer(ITIMER_REAL, NULL, NULL) == -1)
		err_sys("setitimer");
}

static void dlinfo_alarm_launch(void)
{
	struct itimerval new;

	new.it_interval.tv_sec = 1;
	new.it_interval.tv_usec = 0;
	new.it_value.tv_sec = 1;
	new.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &new, NULL) == -1)
		err_sys("setitimer");
}

/*
 * Use this thread to download partial data.
 */
static void *download(void *arg)
{
	int btimes = 0;		/* block times */
	int orig_no;
	ssize_t orig_start, orig_end;
	struct dlpart **dp = NULL;
	struct dlinfo *dl = NULL;

	/* Unpacket the struct packet_args. */
	UNPACKET_ARGS((struct packet_args *)arg, dl, dp, orig_start,
			orig_end, orig_no);
	PACKET_ARGS_FREE((struct packet_args *)arg);
	
	if (!(*dp = dlpart_new(dl, orig_start, orig_end, orig_no))) {
		err_msg("error, try download range: %ld - %ld again",
						orig_start, orig_end);
		return NULL;
	}

	err_dbg(1, "\nthreads %ld starting to download range: %ld-%ld\n",
					(long)pthread_self(),
					(*dp)->dp_start, (*dp)->dp_end);
	/* 
	 * Updating the counter of current running threads. and write the
	 * remaining data which in the header to local file.
	 */
	SAFE_OPERATION(threads_curr++;)


	(*dp)->write(*dp, &total_read, &bytes_per_sec);

	while ((*dp)->dp_start < (*dp)->dp_end) {
		(*dp)->read(*dp);

		/*
		 * If errno is EAGAIN or EWOULDBLOCK, it is a due to error.
		 */
		if ((*dp)->dp_nrd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (btimes++ >= DLINFO_TRYTIMES_MAX)
					goto try_connect_again;
				usleep(1000000 + btimes * 100000);
				continue;
			}

			err_sys("read");
		} else if ((*dp)->dp_nrd == 0) {
			/*
			 * Sava download range, delete the old dp pointer.
			 * and try to etablish a new connection which
			 * returned by the (*dp)->dp_remote.
			 */
try_connect_again:
			btimes = 0;
			orig_start = (*dp)->dp_start;
			orig_end = (*dp)->dp_end;
			orig_no = (*dp)->dp_no;
			(*dp)->delete(*dp);

			*dp = dlpart_new(dl, orig_start, orig_end, orig_no);
			if (!*dp) {
				err_msg("\nremaining bytes %ld - %ld need to "
					"download", orig_start, orig_end);
				goto out;
			}
		}

		btimes = 0;
		(*dp)->write(*dp, &total_read, &bytes_per_sec);

	}
	
	/* 
	 * Subtract the current finished thread. because following line is put
	 * in the front of 'threads_curr--', this may cause 'threads_curr'
	 * greater thant 'threads_total' at a moment.
	 */
	SAFE_OPERATION(threads_total--;)
out:
	SAFE_OPERATION(threads_curr--;)
	return NULL;
}

static int dlinfo_range_generator(struct dlinfo *dl)
{
	int i, s;
	ssize_t pos = 0;
	ssize_t size = dl->di_length / dl->di_nthreads;
	ssize_t start;
	ssize_t end;
	struct dlthreads **dt = &dl->di_threads;

	/* Initial the number of all threads */
	threads_total = dl->di_nthreads;
	for (i = 0; i < dl->di_nthreads; i++) {
		struct packet_args *pkt;

		/* if dt is second pointer in the linked, dt = dt->next.
		 * we need to malloc a block memory for it.
		 */
		if (!*dt) {
			if (!(*dt = malloc(sizeof(**dt))))
				return -1;
		}

		(*dt)->next = NULL;
		start = pos;
		pos += size;
		if (i != dl->di_nthreads - 1)
			end = pos - 1;
		else
			end = dl->di_length - 1;

		/* 
		 * Save the arguments which will pass into download().
		 * especially, the address of (*dt)->dp. this may changed in
		 * the download() by dlpart_new().
		 */
		PACKET_ARGS(pkt, dl, &(*dt)->dp, start, end, i);
		if (!pkt)
			err_exit("packet arguments");

		if ((s = pthread_create(&(*dt)->thread, NULL,
					download, pkt)) != 0) {
			errno = s;
			err_exit("pthread_create");
		}

		dt = &((*dt)->next);
	}
	return 0;
}

void dlinfo_launch(struct dlinfo *dl)
{
	int s;
	struct dlthreads *dt;

	/* 
	 * Before we create threads to start download, we set the download
	 * prompt first. and set the alarm too.
	 */
	dlinfo_set_prompt(dl);
	dlinfo_register_signal_handler();
	dlinfo_alarm_launch();

launch:
	/* 
	 * Set offset of the file to the start of records, and recovery
	 * number of threads to dl->di_nthreads.
	 */
	if (dorecovery) {
		/* 
		 * if file has exist, and it's length is equal to bytes
		 * which need download bytes. so it has download finished.
		 */
		if (dlinfo_download_is_finished(dl))
			return;

		/* can't recovery records normally, try NOT use records again */
		if (dlinfo_records_recovery_all(dl) == -1)
			goto try_ignore_records_again;
	} else {
		if (dlinfo_range_generator(dl) == -1)
			return;
	}

	/* Waiting the threads that we are created above. */
	dt = dl->di_threads;
	while (dt) {
		/*
		 * Since we put all dlpart_new() into the download(). when
		 * all pthread_create() done, only ensure the dt->thread is
		 * set by pthread_create().
		 */
		if (dt->thread) {
			if ((s = pthread_join(dt->thread, NULL)) != 0) {
				errno = s;
				err_sys("pthread_join");
			}
		}
		dt = dt->next;
	}

	/* 
	 * Force the flush the output prompt, and clear the timer, and
	 * removing the records which we written in the end of file.
	 */
	dlinfo_sigalrm_handler(SIGALRM);
	dlinfo_sigalrm_detach();
	if (0 == threads_total)
		dlinfo_records_removing(dl);
	printf("\n");
	return;

try_ignore_records_again:
	if (try_ignore_records) {
		try_ignore_records = 0;
		dorecovery = 0;

		if (close(dl->di_local) == -1)
			err_sys("close");
		if (remove(dl->di_filename) == -1)
			err_sys("remove");

		dlinfo_open_local_file(dl);
		goto launch;
	}
}

void dlinfo_delete(struct dlinfo *dl)
{
	struct dlthreads *dt = dl->di_threads;

	/* close the local file descriptor */
	if (dl->di_local >= 0 && close(dl->di_local) == -1)
		err_sys("close");

	while (dt) {
		if (dt->dp) {
			dt->dp->delete(dt->dp);
		}
		dt = dt->next;
	}
	free(dl);
}

struct dlinfo *dlinfo_new(char *url, char *filename, int nthreads)
{
	int try_times = 0;
	struct dlinfo *dl;

	if ((dl = malloc(sizeof(*dl)))) {
		total = 0;
		total_read = 0;
		bytes_per_sec = 0;
		threads_curr = 0;
		threads_total = 0;
		dorecovery = 0;	/* reset this flags for each download */
		try_ignore_records = 0;

		memset(dl, 0, sizeof(*dl));
		strcpy(dl->di_serv, "http");	/* default port number  */
		strcpy(dl->di_url, url);
		if (filename)
			strcpy(dl->di_filename, filename);
		dl->di_nthreads = nthreads;

		dl->connect = dlinfo_connect;
		dl->launch = dlinfo_launch;
		dl->delete = dlinfo_delete;

		/* 
		 * parsing the url, for remote server's service and hostname.
		 * and establish a temporary connection used to send HTTP HEAD
		 * request, that we can retriving the length and filename.
		 */
		dlinfo_init(dl);
dlinfo_new_again:
		dl->di_remote = dlinfo_connect(dl);
		dlinfo_send_request(dl);
		if (dlinfo_recv_and_parsing(dl) == -1) {
			if (close(dl->di_remote) == -1)
				err_sys("close remote file descriptor");
			if (try_times++ < DLINFO_NEW_TRYTIMES_MAX)
				goto dlinfo_new_again;
			goto dlinfo_new_failure;
		}

		err_dbg(1, "filename: %s, length: %ld\n", dl->di_filename,
			(long)dl->di_length);
		dlinfo_open_local_file(dl);

		total = dl->di_length;	/* Set global variable 'total' */

		/* close temporary connection. */
		if (close(dl->di_remote) == -1)
			err_sys("close remote file descriptor");

		return dl;
	}

dlinfo_new_failure:
	dl->delete(dl);
	return NULL;	/* create the object of struct download failed */
}
