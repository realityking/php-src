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

/* $Id$ */

#ifndef UTF8_H
#define UTF8_H

#ifdef PHP_WIN32
#	define PHP_UTF8_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_UTF8_API __attribute__ ((visibility("default")))
#else
#	define PHP_UTF8_API
#endif

#include <stdint.h>
#include <string.h>
#include "zend.h"

typedef uint8_t char8_t;
typedef uint16_t char16_t;

/* Check whether the given string is valid UTF-8 */
PHP_UTF8_API zend_bool utf8_is_valid(const char8_t *str, size_t str_size);

/* Converts an ISO 8859-1 encoded string into an UTF-8 encoded string.
	result_str has to be freed by the caller */
PHP_UTF8_API void iso88591_to_utf8(const char *str, int str_len, char8_t **result_str, int *result_len);

/* Converts an UTF-8 encoded string into an ISO 8859-1 encoded string.
	Codepoints that are invalid or can't be represented are replaced with a question mark.
	result_str has to be freed by the caller */
PHP_UTF8_API void utf8_to_iso88591(const char8_t *str, int str_len, char **result_str, int *result_len);

/* Converts an UTF-8 encoded string into an ASCII encoded string.
	Codepoints that are invalid or can't be represented are replaced with a question mark.
	result_str has to be freed by the caller */
PHP_UTF8_API void utf8_to_ascii(const char8_t *str, int str_size, char **result_str, int *result_len);

/* Converts an UTF-8 encoded string into an UTF-16 encoded string.
	result_str has to be freed by the caller */
PHP_UTF8_API void utf8_to_utf16(const char8_t *str, size_t str_size, char16_t **result_str, size_t *result_size, zend_bool *valid);


#endif	/* UTF8_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
