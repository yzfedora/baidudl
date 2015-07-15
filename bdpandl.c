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
#include "dlinfo.h"
#include "err_handler.h"


static void usage(const char *progname)
{
	if (progname && *progname)
		fprintf(stderr, "Usage: %s [option] url\n", progname);
	fprintf(stderr,
		"    -n threads   specify the number of thread to download\n"
		"    -o filename  specify the filename of output\n"
		"    -h	          display the help\n\n");
	exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
	int opt, nthreads = 5;	/* default using 4 threads to download */
	char *filename = NULL;
	struct dlinfo *dl;

	while ((opt = getopt(argc, argv, "n:o:h")) != -1) {
		switch (opt) {
		case 'n':
			/* for some reason: pan.baidu.com support 5 maximum
			 * connection to download. */
			nthreads = strtol(optarg, NULL, 10);
			break;
		case 'o':
			filename = optarg;
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if (argc != optind + 1)
		usage(argv[0]);

	if (NULL == (dl = dlinfo_new(argv[optind], filename, nthreads)))
		return -1;

	dl->launch(dl);
	dl->delete(dl);

	return 0;
}
