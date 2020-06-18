#include <stdlib.h>
#include <string.h>

/*
 * base64_decode - Base64 decode
 *
 * This accepts most variations of base-64 encoding, including:
 *    * with or without line breaks
 *    * with or without the final group padded with '=' or '_' characters
 * (The most economical Base-64 variant does not pad the last group and
 * omits line breaks; RFC1341 used for MIME requires both.)
 */
char *
base64_decode(const char *s, size_t len, size_t *out_len)
{
	static const unsigned char digits[64] = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b',
		'c','d','e','f','g','h','i','j','k','l','m','n','o','p',
		'q','r','s','t','u','v','w','x','y','z','0','1','2','3',
		'4','5','6','7','8','9','+','/' };
	static unsigned char decode_table[128];
	char *out, *d;
	const unsigned char *src = (const unsigned char *)s;

	/* If the decode table is not yet initialized, prepare it. */
	if (decode_table[digits[1]] != 1) {
		unsigned i;
		memset(decode_table, 0xff, sizeof(decode_table));
		for (i = 0; i < sizeof(digits); i++)
			decode_table[digits[i]] = i;
	}

	/* Allocate enough space to hold the entire output. */
	/* Note that we may not use all of this... */
	out = (char *)malloc(len - len / 4 + 1);
	if (out == NULL) {
		*out_len = 0;
		return (NULL);
	}
	d = out;

	while (len > 0) {
		/* Collect the next group of (up to) four characters. */
		int v = 0;
		int group_size = 0;
		while (group_size < 4 && len > 0) {
			/* '=' or '_' padding indicates final group. */
			if (*src == '=' || *src == '_') {
				len = 0;
				break;
			}
			/* Skip illegal characters (including line breaks) */
			if (*src > 127 || *src < 32
			    || decode_table[*src] == 0xff) {
				len--;
				src++;
				continue;
			}
			v <<= 6;
			v |= decode_table[*src++];
			len --;
			group_size++;
		}
		/* Align a short group properly. */
		v <<= 6 * (4 - group_size);
		/* Unpack the group we just collected. */
		switch (group_size) {
		case 4: d[2] = v & 0xff;
			/* FALLTHROUGH */
		case 3: d[1] = (v >> 8) & 0xff;
			/* FALLTHROUGH */
		case 2: d[0] = (v >> 16) & 0xff;
			break;
		case 1: /* this is invalid! */
			break;
		}
		d += group_size * 3 / 4;
	}

	*out_len = d - out;
	return (out);
}

/*
 * Encode a sequence of bytes into a C string using base-64 encoding.
 *
 * Returns a null-terminated C string allocated with malloc(); caller
 * is responsible for freeing the result.
 */
char *
base64_encode(const char *s, size_t len)
{
	static const char digits[64] =
	    { 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
	      'P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d',
	      'e','f','g','h','i','j','k','l','m','n','o','p','q','r','s',
	      't','u','v','w','x','y','z','0','1','2','3','4','5','6','7',
	      '8','9','+','/' };
	int v;
	char *d, *out;

	/* 3 bytes becomes 4 chars, but round up and allow for trailing NUL */
	out = (char *)malloc((len * 4 + 2) / 3 + 1);
	if (out == NULL)
		return (NULL);
	d = out;

	/* Convert each group of 3 bytes into 4 characters. */
	while (len >= 3) {
		v = (((int)s[0] << 16) & 0xff0000)
		    | (((int)s[1] << 8) & 0xff00)
		    | (((int)s[2]) & 0x00ff);
		s += 3;
		len -= 3;
		*d++ = digits[(v >> 18) & 0x3f];
		*d++ = digits[(v >> 12) & 0x3f];
		*d++ = digits[(v >> 6) & 0x3f];
		*d++ = digits[(v) & 0x3f];
	}
	/* Handle final group of 1 byte (2 chars) or 2 bytes (3 chars). */
	switch (len) {
	case 0: break;
	case 1:
		v = (((int)s[0] << 16) & 0xff0000);
		*d++ = digits[(v >> 18) & 0x3f];
		*d++ = digits[(v >> 12) & 0x3f];
		break;
	case 2:
		v = (((int)s[0] << 16) & 0xff0000)
		    | (((int)s[1] << 8) & 0xff00);
		*d++ = digits[(v >> 18) & 0x3f];
		*d++ = digits[(v >> 12) & 0x3f];
		*d++ = digits[(v >> 6) & 0x3f];
		break;
	}
	/* Add trailing NUL character so output is a valid C string. */
	*d = '\0';
	return (out);
}
