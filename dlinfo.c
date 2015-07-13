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
static char prompt[DLINFO_PROMPT_SZ];
static ssize_t total,
	       total_read,
	       bytes_per_sec;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Initial the service and host of the server.
 */
static char *dlinfo_init(struct dlinfo *dl)
{
	char *start, *end,
	     *ret = dl->di_host,
	     *url = dl->di_url;

	start = strstr(url, "://");
	if (start == NULL) {
		start = url;
	} else {	/* parsing service name	*/
		memccpy(dl->di_serv, url, ':', 64);
		*(dl->di_serv + strlen(dl->di_serv) - 1) = 0;
		start += 3;
	}
	
	end = strstr(start, "/");
	if (end == NULL)
		end = url + strlen(url);

	strncpy(ret, start, end - start);
	
	debug("ret: %s\n", ret);
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
	
	if (ai == NULL)
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

	printf("Filename: %s, Length: %ld\n", dl->di_filename,
		(long)dl->di_length);
	//strcpy(s, tmp);
}

/*
 * Send HTTP HEAD request to server for retriving the length and filename
 * of the file.
 */
static void dlinfo_send_request(struct dlinfo *dl)
{
	char buf[DLINFO_REQ_SZ];

	sprintf(buf,	"HEAD %s HTTP/1.0\n"
			"Host: %s\r\n\r\n",
			dl->di_url, dl->di_host);
	nwrite(dl->di_remote, buf, strlen(buf));
}

/*
 * Receive the response from the server, which include the HEAD info. And
 * store result to dl->di_length, dl->di_filename.
 */
static void dlinfo_recv_and_parsing(struct dlinfo *dl)
{
	int n, code;
	char buf[DLINFO_RCV_SZ], *p;

	if ((n = read(dl->di_remote, buf, DLINFO_RCV_SZ - 1)) == -1)
		err_exit(errno, "read");

	buf[n] = 0;
	/* response code valid range [200-300) */
	code = retcode(buf);
	if (code < 200 || code >= 300) {
		err_exit(0, "HEAD request receive unknown code %d", code);
	}

#define _CONTENT_LENGTH	"Content-Length: "
	if ((p = strstr(buf, _CONTENT_LENGTH)) != NULL) {
		dl->di_length = strtol(p + sizeof(_CONTENT_LENGTH) - 1,
				NULL, 10);
	}

#define _FILENAME	"filename="
	if ((p = strstr(buf, _FILENAME)) != NULL) {
		if ((p = memccpy(dl->di_filename, p + sizeof(_FILENAME) - 1,
				'\n', 256)) == NULL)
			err_msg(errno, "memccpy");

	}
}

/*
 * Open a file descriptor for store the download data, and store the
 * file descriptor to variable dl->di_local.
 */
static int dlinfo_open_local_file(struct dlinfo *dl)
{
	int *fd = &dl->di_local;
	int flags = O_WRONLY | O_CREAT | O_TRUNC;

	if ((*fd = open(dl->di_filename, flags, 0600)) == -1)
		err_exit(errno, "open");

	return *fd;
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
		int flags = 1;
		ssize_t size;
		
		size = total;
		while (size > 1024) { size >>= 10; flags <<= 1; }
		sprintf(prompt, "download : %s, size %ld%s ",
			dl->di_filename, size,
			(flags == 1) ? "Bytes" :
			(flags == 2) ? "KB" :
			(flags == 4) ? "MB" :
			(flags == 8) ? "GB" :
			"TB");
		return prompt;
}

static void dlinfo_alarm_handler(int signo)
{
	int flags = 1;
	ssize_t speed = bytes_per_sec;

#define PROGRESS_PRINT_WS "                                      "
	while (speed > 1024) {
		speed >>= 10;
		flags <<= 1;
	}
	printf("\r"PROGRESS_PRINT_WS PROGRESS_PRINT_WS);
	printf("\r[%-60s   %4ld%s/s  %2.1f%%]", prompt,
			(long)speed,
			(flags == 1) ? "B" :
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

static void dlinfo_alarm_launch(void)
{
	struct itimerval new;

	new.it_interval.tv_sec = 1;	
	new.it_interval.tv_usec = 0;	
	new.it_value.tv_sec = 1;
	new.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &new, NULL) == -1)
		err_exit(errno, "setitimer");
}

/*
 * Use this thread to download partial data.
 */
static void *download(void *arg)
{
	int s;
	ssize_t orig_start, orig_end;
	struct dlpart *dp = (struct dlpart *)arg;
	struct dlinfo *dl = dp->dp_info;


	printf("\nthread %ld starting to download range: %ld-%ld\n",
			(long)pthread_self(), dp->dp_start, dp->dp_end);

	dp->write(dp);	/* write remaining data in the header. */
	while (dp->dp_start < dp->dp_end) {
		/*
		 * only lock the dp->read call is necessary, since it may
		 * update the global varibale total_read and bytes_per_sec.
		 */
		if ((s = pthread_mutex_lock(&mutex)) != 0)
			err_msg(s, "pthread_mutex_lock");

		dp->read(dp, &total_read, &bytes_per_sec);

		if ((s = pthread_mutex_unlock(&mutex)) != 0)
			err_msg(s, "pthread_mutex_unlock");


		/*
		 * If errno is EAGAIN or EWOULDBLOCK, it is a due to error.
		 */
		if (dp->dp_nrd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;

			err_msg(errno, "read");
		} else if (dp->dp_nrd == 0) {
			/*
			 * Sava download range, delete the old dp pointer.
			 * and try to etablish a new connection which
			 * returned by the dp->dp_remote.
			 */
			orig_start = dp->dp_start;
			orig_end = dp->dp_end;
			dp->delete(dp);

			dp = dlpart_new(dl, orig_start, orig_end);
			if (dp == NULL)
				err_exit(errno, "dlpart_new");

			printf("\nthread %ld starting to download range: %ld-%ld\n",
				(long)pthread_self(), dp->dp_start, dp->dp_end);
		}

		/*
		 * If all things is ok, no error, no timedout. just write it.
		 */
		dp->write(dp);
	}
	
	dp->delete(dp);
	return NULL;
}

void dlinfo_launch(struct dlinfo *dl)
{
	int i, s;
	ssize_t pos = 0, part_size = dl->di_length / dl->di_nthread;
	ssize_t start, end;
	struct dlpart *dp;
	pthread_t *thread;
	
	thread = (pthread_t *)malloc(sizeof(*thread) * dl->di_nthread);
	if (NULL == thread)
		err_exit(errno, "malloc");
	
	/* Before we create threads to start download, we set the download
	 * prompt first. and set the alarm too. */
	dlinfo_set_prompt(dl);
	dlinfo_register_alarm_handler();
	dlinfo_alarm_launch();

	for (i = 0; i < dl->di_nthread; i++) {
		start = pos;
		end = ((i != dl->di_nthread - 1) ? (pos + part_size - 1) :
				(dl->di_length - 1));

		if ((dp = dlpart_new(dl, start, end)) == NULL)
			err_exit(errno, "dlpart_new");
		
		if ((s = pthread_create(&thread[i], NULL, download, dp)) != 0)
			err_exit(s, "pthread_create");
		
		pos += part_size;
	}
	
	/* Waiting the threads that we are created above. */
	for (i = 0; i < dl->di_nthread; i++) {
		if ((s = pthread_join(thread[i], NULL)) != 0)
			err_msg(s, "pthread_join");
	}

	dlinfo_alarm_handler(0);
	printf("\n----------------download finished--------------------\n");
	free(thread);
}

void dlinfo_delete(struct dlinfo *dl)
{
	if (close(dl->di_local) == -1)
		err_msg(errno, "close");
	free(dl);
}

struct dlinfo *dlinfo_new(char *url, int nthread)
{
	struct dlinfo *dl;

	if ((dl = (struct dlinfo *)malloc(sizeof(*dl))) != NULL) {
		memset(dl, 0, sizeof(*dl));
		strcpy(dl->di_serv, "http");	/* default port number	*/
		strcpy(dl->di_url, url);
		dl->di_nthread = nthread;

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
		dlinfo_recv_and_parsing(dl);
		dlinfo_filename_decode(dl);
		dlinfo_open_local_file(dl);

		total = dl->di_length;/* Set global variable 'total' */
		if (close(dl->di_remote) == -1)/* close temporary connection */
			err_exit(errno, "close");

		return dl;
	}

	return NULL;	/* create the object of struct download failed */
}
