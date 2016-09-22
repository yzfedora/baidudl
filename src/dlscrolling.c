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
/* #define ROLL_DISPLAY_TEST	*/
#ifdef ROLL_DISPLAY_TEST
# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
#endif

#include <string.h>
#include "dlscrolling.h"

static char *dlscrolling_orig;	/* Pointer to start of string. */
static char *dlscrolling_curr;	/* Pointer to current string. */

/* Length of original string. */
static unsigned int dlscrolling_orig_size;

/* Window size of scrolling display. */
static unsigned int dlscrolling_window_size;

/* Used width when display in terminal.(ASCII use 1, non-ASCII use 2) */
static unsigned int dlscrolling_length_max;

/*
 * Return the number of characters(displayed characters) in the string utf.
 */
static unsigned int utf8_char_length(char *utf)
{
	unsigned int ulen = 0;
	char u = *utf;

	while (u & 0x80) {
		ulen++;
		u <<= 1;
	}
	return (ulen ? ulen : 1);
}

/*
 * Initialize the dlscrolling_* to pointer s, and the initialize the
 * window size of used scrolling display. 0 returned on success, or -1 on
 * error.
 */
int dlscrolling_init(char *s, unsigned int window_size)
{
	if (!s)
		return -1;
	char *ptr = s;
	int tmp;

	dlscrolling_orig = s;
	dlscrolling_curr = s;
	dlscrolling_window_size = window_size;
	dlscrolling_orig_size = strlen(s);

	while (*ptr) {
		tmp = utf8_char_length(ptr);
		dlscrolling_length_max += (1 == tmp? 1 : 2) ;
		ptr += tmp;
	}
	return 0;
}

/*
 * Set the window size to new, and return old window size.
 */
unsigned int dlscrolling_setsize(unsigned int winsz)
{
	unsigned int save = dlscrolling_window_size;
	dlscrolling_window_size = winsz;
	return save;
}

/*
 * Return a pointer to start of current string, and set the suitable maximum
 * of can be displayed.(*len <= dlscrolling_window_size).
 */
char *dlscrolling_ptr(unsigned int *len, unsigned int *padding)
{
	*len = *padding = 0;

	/*
	 * Both dlscrolling_length_max and dlscrolling_window_size
	 * are represents display units be used.
	 */
	if (dlscrolling_length_max <= dlscrolling_window_size) {
		if (len)
			*len = dlscrolling_orig_size;

		if (padding)
			*padding = dlscrolling_window_size -
						dlscrolling_length_max;
		return dlscrolling_orig;
	}

	char *ret = dlscrolling_curr;
	char *tmp = dlscrolling_curr;
	unsigned int l = 0;
	unsigned int t;

	/* Assume non-ascii characters use 2 width of ascii to display. */
	while (l < dlscrolling_window_size && *tmp) {
		t = utf8_char_length(tmp);
		l += (1 == t) ? 1 : 2;
		*len += t;
		tmp += t;
	}


	/* If display next non-ascii character will use width greater than
	 * specified '*len' */
	if (l > dlscrolling_window_size) {
		*len -= t;
		l -= (1 == t) ? 1 : 2;
	}

	if (l < dlscrolling_window_size && padding)
		*padding = dlscrolling_window_size - l;

	if (likely(*tmp))
		dlscrolling_curr += utf8_char_length(
					dlscrolling_curr);
	else
		dlscrolling_curr = dlscrolling_orig;

	return ret;
}

#ifdef ROLL_DISPLAY_TEST
int main(int argc, char *argv[])
{
	char *test = "Hello, John! 我是柯南，你好嗄！～～～";
	int i, len, window_size = 10;
	char *ptr;

	if (argc == 2)
		test = argv[1];
	if (argc == 3)
		window_size = atoi(argv[2]);

	dlscrolling_init(test, window_size);
	for (i = 0; i <= 100; i++) {
		ptr = dlscrolling_ptr(&len);
		printf("[\e[4;44m%.*s\e[0m]\n", len, ptr);
		sleep(1);
	}
	return 0;
}

#endif
