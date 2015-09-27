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
#include "roll_display.h"

static char *roll_display_orig;
static char *roll_display_curr;
static int roll_display_window_size;	/* Window size of display. */
static int roll_display_length_max;	/* Total used width of display.
					   (ascii use 1, non-ascii use 2) */
static int roll_display_orig_size;	/* Length of the original string. */

static int utf8_char_length(char *utf)
{
	int ulen = 0;
	char u = *utf;

	while (u & 0x80) {
		ulen++;
		u <<= 1;
	}
	return (ulen ? ulen : 1);
}

int roll_display_init(char *s, int window_size)
{
	if (!s || window_size <= 0)
		return -1;
	char *ptr = s;
	int tmp;

	roll_display_orig = s;
	roll_display_curr = s;
	roll_display_window_size = window_size;
	roll_display_orig_size = strlen(s);
	
	while (*ptr) {
		tmp = utf8_char_length(ptr);
		roll_display_length_max += (1 == tmp? 1 : 2) ;
		ptr += tmp;
	}
	return 0;
}

/* Return a pointer to start of current string, and set the suitable maximum
 * of can be displayed.(*len <= roll_display_window_size) */
char *roll_display_ptr(int *len, int *padding)
{
	*len = *padding = 0;
	if (roll_display_length_max <= roll_display_window_size) {
		if (len)
			*len = roll_display_orig_size;

		if (padding)
			*padding = roll_display_window_size -
						roll_display_length_max;
		return roll_display_orig;
	}

	char *ret = roll_display_curr;
	char *tmp = roll_display_curr;
	int l = 0;
	int t;

	/* Assume non-ascii characters use 2 width of ascii to display. */
	while (l < roll_display_window_size && *tmp) {
		t = utf8_char_length(tmp);
		l += (1 == t) ? 1 : 2;
		*len += t;
		tmp += t;
	}


	/* If display next non-ascii character will use width greater than
	 * specified '*len' */
	if (l > roll_display_window_size) {
		*len -= t;
		l -= (1 == t) ? 1 : 2;
		if (padding)
			*padding = roll_display_window_size - l;
	}

	if (likely(*tmp))
		roll_display_curr += utf8_char_length(roll_display_curr);
	else
		roll_display_curr = roll_display_orig;

	return ret;
}

#ifdef ROLL_DISPLAY_TEST
int main(int argc, char *argv[])
{
	char *test = "Hello, John! 我是柯南，你好嗄！～～～";
	int i, len, window_size = 10;
	char *ptr;


	//for (ptr = test; *ptr; ptr += len) {
	//	len = utf8_char_length(ptr);
	//	printf("%.*s, length=%d\n", len, ptr, len);
	//}
	if (argc == 2)
		test = argv[1];
	if (argc == 3)
		window_size = atoi(argv[2]);

	roll_display_init(test, window_size);
	for (i = 0; i <= 100; i++) {
		ptr = roll_display_ptr(&len);
		printf("[\e[4;44m%.*s\e[0m]\n", len, ptr);
		sleep(1);
	}
	return 0;
}

#endif
