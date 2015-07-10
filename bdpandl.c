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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <errno.h>
#include "err_handler.h"

#define BUFSZ	4096
struct dlpart {
	int	dp_remote;	/* remote server file descriptor */
	int	dp_local;	/* local file descriptor */
	ssize_t	dp_start;
	ssize_t	dp_end;
	char	*dp_url;
	char	*dp_host;
};

struct dlinfo {
	int	di_remote;
	int	di_local;
	ssize_t	di_length;	/* total length of the file */
	char	di_filename[BUFSZ];

	int	di_nthread;	/* number of threads to download */
	char	di_serv[64];	/* service type */
	char	di_host[256];	/* host name or IP address */
	char	di_url[BUFSZ];/* oritinal download url request */
	
	void (*start)(struct dlinfo *);
	void (*delete)(struct dlinfo *);
};

/* Using to print the progress, percent... of download info */
static char prompt[BUFSZ];
static ssize_t total,
	       total_read,
	       bytes_per_sec;

static void usage(const char *progname) __attribute__((noreturn));

static void usage(const char *progname)
{
	if (progname && *progname)
		fprintf(stderr, "Usage: %s [option] url\n", progname);
	fprintf(stderr,
		"    -n threads   specify the number of thread to download\n"
		"    -h	          display the help\n\n");
	exit(EXIT_FAILURE);
}

static void nwrite(int fd, const void *buf, unsigned int len)
{
	unsigned int n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n == -1) {
			if (errno == EPIPE)
				err_exit(errno, "nwrite");
			err_msg(errno, "nwrite");
			continue;
		}
			
		len -= n;
		buf += n;
	}
}

static void npwrite(int fd, const void *buf, unsigned int len, off_t offset)
{
	unsigned int n;

	while (len > 0) {
		/*debug("pwrite %d bytes >> fd(%d), offset %ld\n",
			len, fd, offset);*/
		n = pwrite(fd, buf, len, offset);
		if (n == -1) {
			if (errno == EPIPE)
				err_exit(errno, "nwrite");
			err_msg(errno, "nwrite");
			continue;
		}
			
		len -= n;
		buf += n;
		offset += n;
	}
}

static void download_http_decode(char *s)
{
	char tmp[BUFSZ];
	int i, j, k, t;

	for (i = j = 0; s[i]; i++, j++) {
		if (s[i] == '%') {
			/* following 2 bytes is hex-decimal of a char */
			t = s[i + 3];
			s[i + 3] = 0;

			k = strtol(s + i + 1, NULL, 16);
			tmp[j] = (char)k;

			s[i + 3] = t;
			i += 2;
		} else {
			tmp[j] = s[i];
		}
	}
	tmp[j] = 0;

	strcpy(s, tmp);
}

static void download_response_checking(char *s)
{
	char *p, *ep;
	int code;

	if ((p = strchr(s, ' ')) != NULL) {
		p += 1;
		code = strtol(p, &ep, 10);
		if (code < 200 || code >= 300)
			err_exit(0, "Bad response %d", code);
	}
}

/*
 * Initial the service and host of the server.
 */
static char *download_init(struct dlinfo *dl)
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
 * Open a file descriptor for store the download data, and store the
 * file descriptor to variable dl->di_local.
 */
static int download_open_local_file(struct dlinfo *dl)
{
	int *fd = &dl->di_local;

	if ((*fd = open(dl->di_filename, O_WRONLY | O_CREAT, 0600)) == -1)
		err_exit(errno, "open");

	return *fd;
}

/*
 * Store the file descriptor to the dl->di_remote which is connected to
 * server already.
 */
static int download_connect(struct dlinfo *dl)
{
	int s;
	struct addrinfo hints, *res, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	debug("dl->di_serv: %s\n", dl->di_serv);
	if ((s = getaddrinfo(dl->di_host, dl->di_serv, &hints, &res)) != 0)
		err_exit(0, "getaddrinfo: %s\n", gai_strerror(s));

	for (ai = res; ai; ai = ai->ai_next) {
		if ((dl->di_remote = socket(ai->ai_family, ai->ai_socktype,
						ai->ai_protocol)) == -1)
			goto try_next_ai;

		if (connect(dl->di_remote, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		
		err_msg(errno, "connect");
		if (close(dl->di_remote) == -1)
			err_msg(errno, "close");

		continue;
try_next_ai:
		err_msg(errno, "socket");
	}
	
	if (ai == NULL)
		err_exit(0, "Couldn't connection to: %s\n", dl->di_host);

	freeaddrinfo(res);
	return dl->di_remote;
}

/*
 * Send HTTP HEAD request to server for retriving the length and filename
 * of the file.
 */
static void download_send_request_head(struct dlinfo *dl)
{
	char hbuf[BUFSZ];

	sprintf(hbuf,	"HEAD %s HTTP/1.1\n"
			"Host: %s\r\n\r\n",
			dl->di_url, dl->di_host);
	nwrite(dl->di_remote, hbuf, strlen(hbuf));
}

/*
 * Receive the response from the server, which include the HEAD info. And
 * store result to dl->di_length, dl->di_filename.
 */
static void download_recv_response_head_parsing(struct dlinfo *dl)
{
	int n;
	char buf[BUFSZ], *p;

	if ((n = read(dl->di_remote, buf, sizeof(buf) - 1)) == -1)
		err_exit(errno, "read");

	buf[n] = 0;
	
	/* response code valid range [200-300) */
	download_response_checking(buf);
#define _CONTENT_LENGTH	"Content-Length: "
	if ((p = strstr(buf, _CONTENT_LENGTH)) != NULL) {
		dl->di_length = strtol(p + sizeof(_CONTENT_LENGTH) - 1,
				NULL, 10);
	}

#define _FILENAME	"filename=\""
	if ((p = strstr(buf, _FILENAME)) != NULL) {
		if ((p = memccpy(dl->di_filename, p + sizeof(_FILENAME) - 1,
				'"', 256)) == NULL)
			err_msg(errno, "memccpy");

		*(dl->di_filename + strlen(dl->di_filename) - 1) = 0;
		/* some filename does include %5B%20... */
		download_http_decode(dl->di_filename);
	}

	debug("Filename: %s, Length: %ld\n", dl->di_filename, (long)dl->di_length);
}

/****************************************************************
 *		Download-Thread implementations			*
 ****************************************************************/

/*
 * Construct a fixed prompt, like: "download foo.zip, size 128MB    16%".
 * and return a pointer to the prompt string.
 */
static char *download_prompt_set(const char *file)
{
		int flags = 1;
		ssize_t size;
		
		size = total;
		while (size > 1024) { size >>= 10; flags <<= 1; }
		sprintf(prompt, "download : %s, size %ld%s ", file, size,
			(flags == 1) ? "Bytes" :
			(flags == 2) ? "KB" :
			(flags == 4) ? "MB" :
			(flags == 8) ? "GB" :
			"TB");
		return prompt;
}

static void download_progress_print(void)
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
}

static void download_signal_handler(int signo)
{
	download_progress_print();
	bytes_per_sec = 0;
	signal(SIGALRM, download_signal_handler);
	alarm(1);
}

/*
 * Send HTTP Header format:
 * 	GET url\r\n HTTP/1.1\r\n
 * 	Range: bytes=x-y
 */
static void download_thread_send_request(struct dlpart *dp)
{
	char sbuf[BUFSZ];

	sprintf(sbuf,	"GET %s HTTP/1.1\n"
			"Host: %s\n"
			"Range: bytes=%ld-%ld\r\n\r\n",
			dp->dp_url, dp->dp_host,
			(long)dp->dp_start, (long)dp->dp_end);

	nwrite(dp->dp_remote, sbuf, strlen(sbuf));
}

static void download_thread_header_parsing(struct dlpart *dp)
{
	int is_header = 1;
	ssize_t n;
	char *data, *p, *ep;
	char buf[BUFSZ];

	while (is_header) {
		if ((n = read(dp->dp_remote, buf, sizeof(buf) - 1)) == -1)
			err_thread_exit(errno, "read");

		if ((data = strstr(buf, "\r\n\r\n")) != NULL) {
			*data = 0;
			data += 4;
			n -= (strlen(buf) + 4);
			is_header = 0;
		} else {
			buf[n] = 0;
		}

#define _CONTENT_RANGE	"Content-Range: bytes "
		if ((p = strstr(buf, _CONTENT_RANGE)) != NULL) {
			/* THERE IS NO RANGE CHECKING */
			dp->dp_start = strtol(p + sizeof(_CONTENT_RANGE) - 1,
					&ep, 10);
			if  (ep && *ep == '-')
				dp->dp_end = strtol(ep + 1, NULL, 10);
		}
	}
	
	debug("Thread: %ld, Range: %ld-%ld\n", (long)pthread_self(),
		(long)dp->dp_start, (long)dp->dp_end);
	
	/* write remaining data to the position 'dp_offset' of local file */
	npwrite(dp->dp_local, data, n, dp->dp_start);
	dp->dp_start += n;
	total_read += n;
}

/*
 * Use this thread to download partial data.
 */
static void *download_thread(void *arg)
{
	ssize_t n;
	char buf[BUFSZ];
	struct dlpart *dp = (struct dlpart *)arg;

	download_thread_send_request(dp);
	download_thread_header_parsing(dp);

	while (dp->dp_start < dp->dp_end) {
		if ((n = read(dp->dp_remote, buf, sizeof(buf) - 1)) == -1)
			err_exit(errno, "read");

		npwrite(dp->dp_local, buf, n, dp->dp_start);
		total_read += n;
		dp->dp_start += n;
		bytes_per_sec += n;
	}
	
	if (close(dp->dp_remote) == -1)
		err_msg(errno, "close");
	free(dp);
	return NULL;
}

void download_start(struct dlinfo *dl)
{
	int i, s;
	ssize_t pos = 0, part_size = dl->di_length / dl->di_nthread;
	pthread_t *thread;
	struct dlpart *dp;
	
	thread = (pthread_t *)malloc(sizeof(*thread) * dl->di_nthread);
	if (NULL == thread)
		err_exit(errno, "malloc");
	
	/* Before we create threads to start download, we set the download
	 * prompt first. and set the alarm too. */
	download_prompt_set(dl->di_filename);
	signal(SIGALRM, download_signal_handler);
	alarm(1);

	
	for (i = 0; i < dl->di_nthread; i++) {
		/* 'dp' pointer should be free by following thread */
		if ((dp = (struct dlpart *)malloc(sizeof(*dp))) == NULL)
			err_exit(errno, "malloc");
	
		dp->dp_remote = download_connect(dl);
		dp->dp_local = dl->di_local;
		dp->dp_url = dl->di_url;
		dp->dp_host = dl->di_host;
		
		/* set position of start, end */
		dp->dp_start = pos;
		if (i != dl->di_nthread - 1)
			dp->dp_end = pos + part_size - 1;
		else
			dp->dp_end = dl->di_length - 1;
		pos += part_size;
		
		if ((s = pthread_create(&thread[i], NULL, download_thread,
				dp)) != 0)
			err_exit(s, "pthread_create");
	}
	
	/* Waiting the threads that we are created above. */
	for (i = 0; i < dl->di_nthread; i++) {
		if ((s = pthread_join(thread[i], NULL)) != 0)
			err_msg(s, "pthread_join");
	}
	free(thread);
}

void download_delete(struct dlinfo *dl)
{
	if (close(dl->di_local) == -1)
		err_msg(errno, "close");
	free(dl);
}

struct dlinfo *download_new(char *url, int nthread)
{
	struct dlinfo *dl;

	if ((dl = (struct dlinfo *)malloc(sizeof(*dl))) != NULL) {
		memset(dl, 0, sizeof(*dl));
		strcpy(dl->di_serv, "http");	/* default port number	*/
		strcpy(dl->di_url, url);

		dl->di_nthread = nthread;
		dl->start = download_start;
		dl->delete = download_delete;
		
		/* parsing the url, for remote server's service and hostname.
		 * and establish a temporary connection used to send HTTP HEAD
		 * request, that we can retriving the length and filename.
		 */
		download_init(dl);
		download_connect(dl);
		download_send_request_head(dl);
		download_recv_response_head_parsing(dl);
		download_open_local_file(dl);

		total = dl->di_length;/* Set global variable 'total' */
		if (close(dl->di_remote) == -1)/* close temporary connection */
			err_exit(errno, "close");

		return dl;
	}

	return NULL;	/* create the object of struct download failed */
}

int main(int argc, char *argv[])
{
	int opt, nthread = 4;	/* default using 4 threads to download */
	struct dlinfo *dl;

	while ((opt = getopt(argc, argv, "n:h")) != -1) {
		switch (opt) {
		case 'n':
			nthread = strtol(optarg, NULL, 10);
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if (argc != optind + 1)
		usage(argv[0]);

	if ((dl = download_new(argv[optind], nthread)) == NULL)
		return -1;

	dl->start(dl);
	dl->delete(dl);

	return 0;
}
