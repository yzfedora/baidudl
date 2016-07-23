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
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE	/* both for glibc version 2.10.0 before and later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <err_handler.h>
#include "dlinfo.h"

#if (defined(__APPLE__) && defined(__MACH__))
# include <malloc/malloc.h>
#endif

static void usage(const char *progname)
{
	if (progname && *progname)
		fprintf(stderr, "Usage: %s [option] url\n", progname);
	fprintf(stderr,
		"    -d level     enable debug level for ouput\n"
		"    -n threads   specify the number of thread to download\n"
		"    -o filename  specify the filename of output\n"
		"    -l file      import urls from file(url may expire)\n"
		"    -h           display the help\n\n");
	exit(EXIT_FAILURE);
}

static void download_from_url(char *url, char *filename, int nts)
{
	struct dlinfo *dl;

	if (!(dl = dlinfo_new(url, filename, nts))) {
		err_msg("failed to download from url: %s", url);
		return;
	}

	dl->launch(dl);
	dl->delete(dl);
}

/* we assuming will procucts the correct output filename, so here ignore it. */
static void download_from_file(const char *listfile, int nts)
{
#define CONFIG_LINE_MAX	4096
	FILE *list;
	char *lineptr = malloc(CONFIG_LINE_MAX);
	size_t max = CONFIG_LINE_MAX;
	ssize_t num = 0;

	if (!(list = fopen(listfile, "r")))
		err_exit("failed to open list file: %s", listfile);

	while (1) {
		errno = 0;	/* ensure no effects by other calls */
		num = getline(&lineptr, &max, list);
		if (num == -1) {
			if (!errno)
				break;	/* end-of-file */
			continue;
		}
		if (lineptr[num - 1] == '\n')
			lineptr[num - 1] = 0;

		/* when this call be down, try next to download */
		download_from_url(lineptr, NULL, nts);
	}

	free(lineptr);
	if (fclose(list))
		err_exit("failed to close list file: %s", listfile);
}

int main(int argc, char *argv[])
{
	int opt, nthreads = 10;	/* default using 4 threads to download */
	char *filename = NULL;
	char *listfile = NULL;	/* which stored the download list */

	while ((opt = getopt(argc, argv, "d:n:o:l:h")) != -1) {
		switch (opt) {
		case 'd':
			/*
			 * default debug is off, current support 1 and 2
			 * debug level.
			 */
			err_setdebug(atoi(optarg));
			break;
		case 'n':
			nthreads = strtol(optarg, NULL, 10);
			break;
		case 'o':
			filename = optarg;
			break;
		case 'l':
			listfile = optarg;
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if ((!listfile && (argc != optind + 1)) ||
			(listfile && (argc != optind)))
		usage(argv[0]);

	if (!listfile)
		download_from_url(argv[optind], filename, nthreads);
	else
		download_from_file(listfile, nthreads);

	return 0;
}
