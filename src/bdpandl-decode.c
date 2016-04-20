#include <stdio.h>
#include <stdlib.h>

#define ENCODE_NAME_MAX	4096

char *string_decode(char *src)
{
	char tmp[ENCODE_NAME_MAX];
	char *s = src;
	int i, j, k, t;

	/* strip the double-quotes */
	for (i = j = k = 0; s[i] && s[i] != '\r' && s[i+1] != '\n'; i++) {
		if (s[i] != '"')
			tmp[j++] = s[i];
		else {
			if (2 == ++k)
				break;
		}
	}
	tmp[j] = 0;

	s = src;
	for (i = j = k = 0; tmp[i]; i++, j++) {
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

	return s;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "bdpandl-decode <Decode-String>\n");
		return 1;
	}
	printf("%s\n", string_decode(argv[1]));
	return 0;
}
