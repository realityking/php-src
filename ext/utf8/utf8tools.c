/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Rouven Weßling                                               |
  +----------------------------------------------------------------------+
*/

#include "utf8.h"
#include "utf8decode.h"

/*
 * Author: Mikko Lehtonen
 * See: https://github.com/scoopr/wtf8/blob/master/wtf8.h
 */
static inline char8_t*
utf8_encode(uint32_t codepoint, const char8_t *str, int *len)
{
	char8_t *ustr = (char8_t*)str;
	*len = 0;
	if (codepoint <= 0x7f) {
		ustr[0] = (char8_t)codepoint;
		ustr += 1;
		*len = 1;
	} else if (codepoint <= 0x7ff ) {
		ustr[0] = (char8_t) (0xc0 + (codepoint >> 6));
		ustr[1] = (char8_t) (0x80 + (codepoint & 0x3f));
		ustr += 2;
		*len = 2;
	} else if (codepoint <= 0xffff) {
		ustr[0] = (char8_t) (0xe0 + (codepoint >> 12));
		ustr[1] = (char8_t) (0x80 + ((codepoint >> 6) & 63));
		ustr[2] = (char8_t) (0x80 + (codepoint & 63));
		ustr += 3;
		*len = 3;
	} else if (codepoint <= 0x1ffff) {
		ustr[0] = (char8_t) (0xf0 + (codepoint >> 18));
		ustr[1] = (char8_t) (0x80 + ((codepoint >> 12) & 0x3f));
		ustr[2] = (char8_t) (0x80 + ((codepoint >> 6) & 0x3f));
		ustr[3] = (char8_t) (0x80 + (codepoint & 0x3f));
		ustr += 4;
		*len = 4;
	}

	return (char8_t*)ustr;
}

PHP_UTF8_API zend_bool
utf8_is_valid(const char8_t *str, size_t str_size)
{
	uint32_t  codepoint;
	uint32_t  state = UTF8_ACCEPT;
	const char8_t  *input_end = str + str_size;

	while (str < input_end) {
		utf8_decode(&state, &codepoint, *str++);
		if (state == UTF8_REJECT) {
			return 0;
		}
	}

	return state == UTF8_ACCEPT;
}

PHP_UTF8_API void
iso88591_to_utf8(const char *str, int str_size, char8_t **result_str, int *result_len)
{
	char8_t *result, *begin;
	uint32_t codepoint;
	int      codepoint_len;
	const char8_t *ustr = (char8_t*)str;
	const char8_t *input_end = ustr + str_size;

	*result_len = 0;
	result = (char8_t*) emalloc(2 * str_size);
	begin = result;

	while (ustr < input_end) {
		if (*ustr < 0x80) {
			*result++ = *ustr;
			*result_len += 1;
		} else {
			*result++ = 0xc2 + (*ustr > 0xbf);
			*result++ = (*ustr & 0x3f) + 0x80;
			*result_len += 2;
		}
		*ustr++;
	}

	begin = (char8_t*) erealloc(begin, *result_len);

	*result_str = begin;
}

static inline void
utf8_to_iso88591_ex(const char8_t *str, int str_size, char **result_str, int *result_len, uint8_t cutoff)
{
	uint32_t codepoint;
	uint32_t current = UTF8_ACCEPT, prev = UTF8_ACCEPT;
	char    *result, *begin;
	const char8_t *input_end = str + str_size;

	*result_len = 0;
	result = (char*) emalloc(str_size);
	begin = result;

	while (str < input_end) {
		prev = current;
		if (!utf8_decode(&current, &codepoint, *str++)) {
			if (codepoint <= cutoff) {
				*result++ = (char8_t) codepoint;
				*result_len += 1;
			} else {
				// Can't map this codepoint. Substitute with a ?
				*result++ = 0x3f;
				*result_len += 1;
			}
		} else if (current == UTF8_REJECT) {
			// Invalid code point. Substitute with a ?
			*result++ = 0x3f;
			*result_len += 1;
			// Reset to UTF8_ACCEPT since we've recovered and retry the last byte
			current = UTF8_ACCEPT;
			if (prev != UTF8_ACCEPT)
				*str--;
		}
	}
	if (current != UTF8_ACCEPT) {
		// Apparently the last codepoint was incomplete
		*result++ = 0x3f;
		*result_len += 1;
	}

	begin = (char*) erealloc(begin, *result_len);

	*result_str = (char*) begin;
}

PHP_UTF8_API void
utf8_to_iso88591(const char8_t *str, int str_size, char **result_str, int *result_len)
{
	utf8_to_iso88591_ex(str, str_size, result_str, result_len, 0xff);
}

PHP_UTF8_API void
utf8_to_ascii(const char8_t *str, int str_size, char **result_str, int *result_len)
{
	utf8_to_iso88591_ex(str, str_size, result_str, result_len, 0x80);
}

PHP_UTF8_API void
utf8_to_utf16(const char8_t *str, size_t str_size, char16_t **result_str, size_t *result_size, zend_bool *valid)
{
	uint32_t       codepoint;
	uint32_t       state = UTF8_ACCEPT;
	int            i = 0;
	char16_t      *result, *begin;
	const char8_t *input_end = str + str_size;

	*result_size = 0;
	result = (char16_t*) emalloc(str_size * sizeof(char16_t));
	begin = result;

	while (str < input_end) {
		if (utf8_decode(&state, &codepoint, *str++)) {
			continue;
		}

		if (codepoint > 0xffff) {
			*result++ = (char16_t)(0xD7C0 + (codepoint >> 10));
			*result++ = (char16_t)(0xDC00 + (codepoint & 0x3FF));
			*result_size += 4;
		} else {
			*result++ = (char16_t)codepoint;
			*result_size += 2;
		}
	}

	*valid = (state == UTF8_ACCEPT);
	begin = (char16_t*) erealloc(begin, *result_size);
	*result_str = begin;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
