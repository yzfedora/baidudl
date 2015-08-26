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
#include <stdlib.h>	/* strtol() */
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

#include "dlinfo.h"
#include "dlpart.h"
#include "dlcommon.h"
#include "err_handler.h"

/* Using to print the progress, percent... of download info */
#define	DLINFO_PROMPT_SZ	1024
int dorecovery;
int try_ignore_records;
static char prompt[DLINFO_PROMPT_SZ];
static ssize_t	total,
		total_read,
		bytes_per_sec;
static long	sig_cnt;
static size_t	filename_len,
		filename_dyn;
static char	*filename_fnm;	/* point to temporary used file name */

/*
 * Initial the service and host of the server. parsed url exclude the trailing
 * '/' character.
 */
static char *dlinfo_init(struct dlinfo *dl)
{
	char *start, *end,
	     *ret = dl->di_host,
	     *url = dl->di_url;

	start = strstr(url, "://");
	if (NULL == start) {
		start = url;
	} else {	/* parsing service name	*/
		memccpy(dl->di_serv, url, ':', DLINFO_SRV_SZ);
		*(dl->di_serv + strlen(dl->di_serv) - 1) = 0;
		start += 3;
	}
	
	end = strstr(start, "/");
	if (NULL == end)
		end = url + strlen(url);

	strncpy(ret, start, end - start);
	return ret;
}

/*
 * Store the file descriptor to the '*sockfd' which is connected to
 * server already. and return the socket file descriptor also.
 */
int dlinfo_connect(struct dlinfo *dl)
{
	int s, fd;
	struct addrinfo hints, *res, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((s = getaddrinfo(dl->di_host, dl->di_serv, &hints, &res)) != 0)
		err_exit(0, "getaddrinfo: %s\n", gai_strerror(s));

	for (ai = res; ai; ai = ai->ai_next) {
		if ((fd = socket(ai->ai_family, ai->ai_socktype,
						ai->ai_protocol)) == -1) {
			err_msg(errno, "socket");
			continue;
		}

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		
		err_msg(errno, "connect");
		if (close(fd) == -1)
			err_msg(errno, "close");

	}
	
	if (NULL == ai)
		err_exit(0, "Couldn't connection to: %s\n", dl->di_host);


	freeaddrinfo(res);
	return fd;
}

/*
 * Used to decode the filename string ''
 */
static void dlinfo_filename_decode(struct dlinfo *dl)
{
	char tmp[DLINFO_NAME_MAX], *s = dl->di_filename;
	int i, j, k, t;

	/* strip the double-quotes */
	for (i = j = 0; s[i] != '\r' && s[i+1] != '\n' && s[i]; i++) {
		if (s[i] != '"')
			tmp[j++] = s[i];
	}
	tmp[j] = 0;

	for (i = j = 0; tmp[i]; i++, j++) {
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

	/*printf("Filename: %s, Length: %ld\n", dl->di_filename,
		(long)dl->di_length);*/
}

/*
 * Send HTTP HEAD request to server for retriving the length and filename
 * of the file.
 */
static void dlinfo_send_request(struct dlinfo *dl)
{
	char buf[DLINFO_REQ_SZ];

	/* not using HEAD request, sometime HEAD will produce 400 error. */
	sprintf(buf,	"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n\r\n"
			geturi(dl->di_url, dl->di_host), dl->di_host);
	nwrite(dl->di_remote, buf, strlen(buf));
	debug("------------------ Send Requst,1 ----------------------\n%s",
		buf);
}

/*
 * Receive the response from the server, which include the HEAD info. And
 * store result to dl->di_length, dl->di_filename.
 */
static int dlinfo_recv_and_parsing(struct dlinfo *dl)
{
	int n, code;
	char buf[DLINFO_RCV_SZ], *p;

	/* any error or end-of-file will cause parsing header fails */
	if ((n = read(dl->di_remote, buf, DLINFO_RCV_SZ - 1)) <= 0)
		return -1;

	buf[n] = 0;
	debug("-------------------Received HEAD info-----------------\n"
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

#define _FILENAME	"filename="
	if ((p = strstr(buf, _FILENAME))) {
		p = memccpy(dl->di_filename, p + sizeof(_FILENAME) - 1,
				'\n', 256);
		if (p) return 0;
		err_msg(errno, "memccpy");
	}

	/* if filename parsing failed, then parsing filename from url. */
	if ((p = strrchr(dl->di_url, '/')))
		strcpy(dl->di_filename, p + 1);

	return 0;
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
		err_exit(errno, "records recovery occur errors");
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

	/* seek to the start of the records. and retriving number of threads.*/
	lseek(dl->di_local, dl->di_length, SEEK_SET);
	dlinfo_records_recovery(dl, &dl->di_nthreads,
			sizeof(dl->di_nthreads));

	/* calculate the records length. it should be equal the ('filelen' -
	 * dl->di_length) */
	recordlen = dl->di_nthreads * 2 * sizeof(ssize_t) +
			sizeof(dl->di_nthreads);


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
	int i;
	ssize_t start, end, nedl = 0;
	struct dlthreads **dt = &dl->di_threads;

	if (dlinfo_records_recovery_nthreads(dl) == -1)
		return -1;
	
	/* this isn't necessary, but for a non-dependencies impl. */
	lseek(dl->di_local, dl->di_length + sizeof(dl->di_nthreads), SEEK_SET);
	for (i = 0; i < dl->di_nthreads; i++) {
		/*
		 * if dt is second pointer in the linked list, dt = dt->next.
		 * we need to malloc a block memory for it.
		 */
		if (NULL == *dt) {
			if (NULL == (*dt = malloc(sizeof(**dt))))
				err_exit(errno, "malloc");
			memset(*dt, 0, sizeof(**dt));
		}

		dlinfo_records_recovery(dl, &start, sizeof(start));
		dlinfo_records_recovery(dl, &end, sizeof(end));
		if (start > end) {
			/* 
			 * if this range is finished, set 'dt->thread' and
			 * 'dt->dp both to 0 or NULL properly'.
			 */
			if (start == end + 1) {
				(*dt)->thread = 0;
				(*dt)->dp = NULL;
				goto next_range;
			}
			err_exit(0, "recovery error range: %ld-%ld\n",
					start, end);
		}

		if (NULL == ((*dt)->dp = dlpart_new(dl, start, end, i)))
			err_exit(errno, "malloc");
		
		nedl += (end - start);
next_range:
		dt = &((*dt)->next);
	}

	total_read = total - nedl;
	return total_read;
}


static void dlinfo_records_removing(struct dlinfo *dl)
{
	if (ftruncate(dl->di_local, dl->di_length) == -1)
		err_exit(errno, "ftruncate");
}

/*
 * Open a file descriptor for store the download data, and store the
 * file descriptor to variable dl->di_local.
 */
static int dlinfo_open_local_file(struct dlinfo *dl)
{
#define PERMS (S_IRUSR | S_IWUSR)
	int fd, ret, flags = O_RDWR | O_CREAT | O_EXCL;

	if ((fd = open(dl->di_filename, flags, PERMS)) == -1) {
		if (errno == EEXIST) {
			/*
			 * same file already exists. try recovery records
			 */
			flags &= ~O_EXCL;
			if ((fd = open(dl->di_filename, flags, PERMS)) == -1)
				err_exit(errno, "open");

			dorecovery = 1;
			goto dlinfo_open_local_file_return;
		}
		err_exit(errno, "open");
	}

	/* If no file exists, append number of threads records to the file.*/
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
	ssize_t size, orig_size;

	orig_size = size = total;
	while (size > 1024) { size >>= 10; flags++; }
	sprintf(prompt, "download: %-30.30s      %.1f%s ",
		dl->di_filename,
		orig_size / ((double)(1 << (10 * flags))),
		(flags == 0) ? "Bytes" :
		(flags == 1) ? "KB" :
		(flags == 2) ? "MB" :
		(flags == 3) ? "GB" :
		"TB");

	/* set this variable used to decision whether dynamically print
	 * the file name. */
	filename_len = strlen(dl->di_filename);

	/* 30 indicate the maximum length of file name. */
	filename_dyn = filename_len - 30 + 1;

	filename_fnm = dl->di_filename;
	sig_cnt = 0;

	return prompt;
}

/* if dl->di_filename is more than 30 bytes, then dynamically print the
 * full name of this file. */
static void dlinfo_set_prompt_dyn(void)
{
	if (filename_len <= 30)
		return;

	sprintf(prompt + 10,/* start position of file name */ "%.30s",
			filename_fnm + (sig_cnt % filename_dyn));
	
	/* recovery the ' ', because of it be set to 0 by sprintf */
	prompt[40] = ',';
	sig_cnt++;
}

static void dlinfo_alarm_handler(int signo)
{
	int flags = 1;
	ssize_t speed = bytes_per_sec;

	dlinfo_set_prompt_dyn();
#define PROGRESS_PRINT_WS "                                      "
	do {
		speed >>= 10;
		flags <<= 1;
	} while (speed > 1024);
	printf("\r"PROGRESS_PRINT_WS PROGRESS_PRINT_WS);
	printf("\r[%-60s  %4ld%s/s %5.1f%%]", prompt,
			(long)speed,
			(flags == 2) ? "KB" :
			(flags == 4) ? "MB" : "GB",
			((float)total_read / total) * 100);

	fflush(stdout);
	bytes_per_sec = 0;
}

static void dlinfo_register_alarm_handler(void)
{
	struct sigaction act, old;

	memset(&act, 0, sizeof(act));
	act.sa_flags |= SA_RESTART;
	act.sa_handler = dlinfo_alarm_handler;

	if (sigaction(SIGALRM, &act, &old) == -1)
		err_exit(errno, "sigaction");
}

static void dlinfo_alarm_detach(void)
{
	if (setitimer(ITIMER_REAL, NULL, NULL) == -1)
		err_msg(errno, "setitimer");
}

static void dlinfo_alarm_launch(void)
{
	struct itimerval new;

	new.it_interval.tv_sec = 1;	
	new.it_interval.tv_usec = 0;	
	new.it_value.tv_sec = 1;
	new.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &new, NULL) == -1)
		err_msg(errno, "setitimer");
}

/*
 * Use this thread to download partial data.
 */
static void *download(void *arg)
{
	int btimes = 0;	/* block times */
	int orig_no;
	ssize_t orig_start, orig_end;
	struct dlpart **dp = (struct dlpart **)arg;
	struct dlinfo *dl = (dp && *dp) ? (*dp)->dp_info : NULL;


	/* this situation will only happen in recovery records, and
	 * encountered the partial ranges are download finished. */
	if (!dp || !*dp)
		return NULL;

	/*printf("\nthreads %ld starting to download range: %ld-%ld\n",
			(long)pthread_self(), (*dp)->dp_start, (*dp)->dp_end);*/

	/* write remaining data in the header. */
	(*dp)->write(*dp, &total_read, &bytes_per_sec);
	while ((*dp)->dp_start < (*dp)->dp_end) {
		(*dp)->read((*dp));

		/*
		 * If errno is EAGAIN or EWOULDBLOCK, it is a due to error.
		 */
		if ((*dp)->dp_nrd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (btimes++ > 30)
					goto try_connect_again;
				sleep(1);
				continue;
			}

			err_msg(errno, "read");
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
			/* if *dp is NULL, occur fatal error. */
			if (NULL == *dp)
				return NULL;
		}

		btimes = 0;
		(*dp)->write(*dp, &total_read, &bytes_per_sec);

	}

	return NULL;
}

static int dlinfo_range_generator(struct dlinfo *dl)
{
	int i;
	ssize_t pos = 0, size = dl->di_length / dl->di_nthreads;
	ssize_t start, end;
	struct dlthreads **dt = &dl->di_threads;

	for (i = 0; i < dl->di_nthreads; i++) {
		/*
		 * if dt is second pointer in the linked, dt = dt->next.
		 * we need to malloc a block memory for it.
		 */
		if (NULL == *dt) {
			if (NULL == (*dt = malloc(sizeof(**dt))))
				return -1;
		}

		start = pos;
		end = (i != dl->di_nthreads - 1) ?
				(pos + size - 1) : (dl->di_length - 1);
		pos += size;

		(*dt)->next = NULL;
		if (NULL == ((*dt)->dp = dlpart_new(dl, start, end, i)))
			return -1;

		dt = &((*dt)->next);
	}
	return 0;
}

void dlinfo_launch(struct dlinfo *dl)
{
	int s;
	struct dlthreads *dt;

dlinfo_launch_start:
	/* Set offset of the file to the start of records, and recovery
	 * number of threads to dl->di_nthreads. */
	if (dorecovery) {
		/* if file has exist, and it's length is equal to bytes
		 * which need download bytes. so it has download finished. */
		if (dlinfo_download_is_finished(dl))
			return;

		/* can't recovery records normally, try NOT use records again */
		if (dlinfo_records_recovery_all(dl) == -1)
			goto dlinfo_launch_again;
	} else {
		if (dlinfo_range_generator(dl) == -1)
			return;
	}

	/* Before we create threads to start download, we set the download
	 * prompt first. and set the alarm too. */
	dlinfo_set_prompt(dl);
	dlinfo_register_alarm_handler();
	dlinfo_alarm_launch();

	dt = dl->di_threads;
	while (NULL != dt) {
		if ((s = pthread_create(&dt->thread, NULL, download,
						&dt->dp)) != 0)
			err_exit(s, "pthread_create");
		dt = dt->next;
	}

	/* Waiting the threads that we are created above. */
	dt = dl->di_threads;
	while (NULL != dt) {
		if (0 != dt->thread && NULL != dt->dp) {
			if ((s = pthread_join(dt->thread, NULL)) != 0)
				err_msg(s, "pthread_join");
		}
		dt = dt->next;
	}

	dlinfo_alarm_handler(SIGALRM);
	dlinfo_alarm_detach();
	dlinfo_records_removing(dl);	/* Removing trailing records */
	printf("\n");
	return;

dlinfo_launch_again:
	if (try_ignore_records) {
		try_ignore_records = 0;
		dorecovery = 0;
		if (close(dl->di_local) == -1)	err_msg(errno, "close");
		if (remove(dl->di_filename) == -1) err_msg(errno, "remove");
		dlinfo_open_local_file(dl);
		goto dlinfo_launch_start;
	}
}

void dlinfo_delete(struct dlinfo *dl)
{
	struct dlthreads *dt = dl->di_threads;

	/* close the local file descriptor */
	if (dl->di_local >= 0 && close(dl->di_local) == -1)
		err_msg(errno, "close");
	
	while (NULL != dt) {
		if (NULL != dt->dp) {
			dt->dp->delete(dt->dp);
		}
		dt = dt->next;
	}
	free(dl);
}

struct dlinfo *dlinfo_new(char *url, char *filename, int nthreads)
{
	struct dlinfo *dl;

	if (NULL != (dl = (struct dlinfo *)malloc(sizeof(*dl)))) {
		total = 0;
		total_read = 0;
		bytes_per_sec = 0;
		dorecovery = 0;	/* reset this flags for each download */
		try_ignore_records = 0;

		memset(dl, 0, sizeof(*dl));
		strcpy(dl->di_serv, "http");	/* default port number	*/
		strcpy(dl->di_url, url);
		if (NULL != filename)
			strcpy(dl->di_filename, filename);
		dl->di_nthreads = nthreads;

		dl->connect = dlinfo_connect;
		dl->launch = dlinfo_launch;		
		dl->delete = dlinfo_delete;
		
		/* parsing the url, for remote server's service and hostname.
		 * and establish a temporary connection used to send HTTP HEAD
		 * request, that we can retriving the length and filename.
		 */
		dlinfo_init(dl);
		dl->di_remote = dlinfo_connect(dl);
		dlinfo_send_request(dl);
		if (dlinfo_recv_and_parsing(dl) == -1)
			goto dlinfo_new_failure;

		dlinfo_filename_decode(dl);
		dlinfo_open_local_file(dl);

		total = dl->di_length;/* Set global variable 'total' */
		/* close temporary connection */
		if (close(dl->di_remote) == -1)
			goto dlinfo_new_failure;

		return dl;
	}

dlinfo_new_failure:
	dl->delete(dl);
	return NULL;	/* create the object of struct download failed */
}
