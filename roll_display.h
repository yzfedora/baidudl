#ifndef _ROLL_DISPLAY_H
#define _ROLL_DISPLAY_H

#define likely(x)	__builtin_expect((x), 1)
#define unlikely(x)	__builtin_expect((x), 0)

int roll_display_init(char *s, int length);
char *roll_display_ptr(int *len, int *padding);
#endif
