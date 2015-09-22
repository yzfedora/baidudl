/* #define ROLL_DISPLAY_TEST	*/
#ifdef ROLL_DISPLAY_TEST
# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
#endif
#include "roll_display.h"

static char *roll_display_orig;
static char *roll_display_curr;
static int roll_display_window_size;
static int roll_display_length_max;

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

void roll_display_init(char *s, int length)
{
	char *ptr = s;
	int tmp;

	roll_display_orig = s;
	roll_display_curr = s;
	roll_display_window_size = length;
	
	while (*ptr) {
		tmp = utf8_char_length(ptr);
		roll_display_length_max += (1 == tmp? 1 : 2) ;
		ptr += tmp;
	}
}

/* Return a pointer to start of current string, and set the suitable maximum
 * of can be displayed.(*len <= roll_display_window_size) */
char *roll_display_ptr(int *len)
{
	if (roll_display_length_max <= roll_display_window_size) {
		*len = roll_display_length_max;
		return roll_display_orig;
	}

	char *ret = roll_display_curr;
	char *tmp = roll_display_curr;
	int l = 0, t;

	/* Assume non-ascii characters use 2 width of ascii to display. */
	*len = 0;
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
