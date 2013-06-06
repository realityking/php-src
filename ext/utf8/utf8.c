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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_utf8.h"
#include "utf8.h"
#include "ext/standard/html.h"
#include "ext/intl/intl_convert.h"
#include "ext/xml/php_xml.h"

ZEND_BEGIN_ARG_INFO_EX(utf8_is_valid_arginfo, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(utf8_to_utf16_arginfo, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(utf8_encode_arginfo, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(utf8_decode_arginfo, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

/* {{{ utf8_functions[]
 *
 * Every user visible function must have an entry in utf8_functions[].
 */
const zend_function_entry utf8_functions[] = {
	PHP_FE(utf8_is_valid,       utf8_is_valid_arginfo)
	PHP_FE(utf8_is_valid_json,  utf8_is_valid_arginfo)
	PHP_FE(utf8_to_utf16,	    utf8_to_utf16_arginfo)
	PHP_FE(utf8_to_utf16_json,	utf8_to_utf16_arginfo)
	PHP_FE(utf8_to_utf16_intl,	utf8_to_utf16_arginfo)
	PHP_FE(utf8_encode,			utf8_encode_arginfo)
	PHP_FE(utf8_decode,			utf8_decode_arginfo)
	PHP_FE(utf8_encode_xml,		utf8_encode_arginfo)
	PHP_FE(utf8_decode_xml,		utf8_decode_arginfo)
	PHP_FE(utf8_ord_old,		utf8_decode_arginfo)
	PHP_FE(utf8_ord_new,		utf8_decode_arginfo)
	PHP_FE_END	/* Must be the last line in utf8_functions[] */
};
/* }}} */

/* {{{ utf8_module_entry
 */
zend_module_entry utf8_module_entry = {
	STANDARD_MODULE_HEADER,
	"utf8",
	utf8_functions,
	NULL,	/* MINIT */
	NULL,	/* MSHUTDOWN */
	NULL,	/* RINIT */
	NULL,	/* RSHUTDOWN */
	PHP_MINFO(utf8),
	"0.1",
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_UTF8
ZEND_GET_MODULE(utf8)
#endif

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(utf8)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "utf8 support", "enabled");
	php_info_print_table_end();
}
/* }}} */

static int utf8_to_utf16_json(unsigned short *utf16, char utf8[], int len) /* {{{ */
{
	size_t pos = 0, us;
	int j, status;

	if (utf16) {
		/* really convert the utf8 string */
		for (j=0 ; pos < len ; j++) {
			us = php_next_utf8_char((const unsigned char *)utf8, len, &pos, &status);
			if (status != SUCCESS) {
				return -1;
			}
			/* From http://en.wikipedia.org/wiki/UTF16 */
			if (us >= 0x10000) {
				us -= 0x10000;
				utf16[j++] = (unsigned short)((us >> 10) | 0xd800);
				utf16[j] = (unsigned short)((us & 0x3ff) | 0xdc00);
			} else {
				utf16[j] = (unsigned short)us;
			}
		}
	} else {
		/* Only check if utf8 string is valid, and compute utf16 lenght */
		for (j=0 ; pos < len ; j++) {
			us = php_next_utf8_char((const unsigned char *)utf8, len, &pos, &status);
			if (status != SUCCESS) {
				return -1;
			}
			if (us >= 0x10000) {
				j++;
			}
		}
	}
	return j;
}
/* }}} */


/* {{{ proto bool utf8_is_valid(string str)
   */
PHP_FUNCTION(utf8_is_valid)
{
	char8_t  *str = NULL;
	int       str_len = 0;
	zend_bool valid = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_TRUE;
	}

	valid = utf8_is_valid(str, str_len);

	RETURN_BOOL(valid);
}
/* }}} utf8_is_valid */

PHP_FUNCTION(utf8_is_valid_json)
{
	char     *str = NULL;
	int       str_len = 0;
	int       result;
	zend_bool valid = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_TRUE;
	}
	
	result = utf8_to_utf16_json(NULL, str, str_len);

	RETURN_BOOL((result >= 0));
}

/* {{{ proto string utf8_to_utf16(string str)
   */
PHP_FUNCTION(utf8_to_utf16_intl)
{
	char *str;
	int   str_len = 0;
	int   result_len = 0;
	UChar *utf16;
	UErrorCode status = U_ZERO_ERROR;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	utf16 = (unsigned short *) safe_emalloc(str_len, sizeof(unsigned short), 0);
	intl_convert_utf8_to_utf16(&utf16, &result_len, str, str_len, &status);

	if (U_FAILURE(status)) {
		php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "Could not calculate result.");
		return;
	}

	RETURN_STRINGL(utf16, result_len, 0);
}
/* }}} utf8_to_utf16 */

/* {{{ proto string utf8_to_utf16(string str)
   */
PHP_FUNCTION(utf8_to_utf16_json)
{
	char *str;
	int      str_len = 0;
	int   result_len = 0;
	zend_bool valid = 0;
	unsigned short *utf16;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	utf16 = (unsigned short *) safe_emalloc(str_len, sizeof(unsigned short), 0);
	result_len = utf8_to_utf16_json(utf16, str, str_len);
	
	if (result_len < 0) {
		php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "Could not calculate result.");
		return;
	}

	RETURN_STRINGL(utf16, result_len, 0);
}
/* }}} utf8_to_utf16 */

/* {{{ proto string utf8_to_utf16(string str)
   */
PHP_FUNCTION(utf8_to_utf16)
{
	char8_t  *str;
	char16_t *result;
	int       str_len = 0;
	size_t    result_len = 0;
	zend_bool valid = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	utf8_to_utf16(str, str_len, &result, &result_len, &valid);
	
	if (!valid) {
		php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "Could not calculate result.");
		return;
	}

	RETURN_STRINGL(result, result_len, 0);
}
/* }}} utf8_to_utf16 */

/* {{{ proto string utf8_encode(string str)
   */
PHP_FUNCTION(utf8_encode)
{
	char    *str;
	int      str_len = 0, result_len = 0;
	char8_t *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	iso88591_to_utf8(str, str_len, &result, &result_len);

	RETURN_STRINGL((char*)result, result_len, 0);
}
/* }}} utf8_encode */

/* {{{ proto string utf8_decode(string str)
   */
PHP_FUNCTION(utf8_decode)
{
	char8_t *str;
	char    *result;
	int      str_len = 0, result_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	utf8_to_iso88591(str, str_len, &result, &result_len);

	RETURN_STRINGL(result, result_len, 0);
}
/* }}} utf8_decode */

/* {{{ proto string utf8_encode(string data) 
   Encodes an ISO-8859-1 string to UTF-8 */
PHP_FUNCTION(utf8_encode_xml)
{
	char *arg;
	XML_Char *encoded;
	int arg_len, len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	encoded = xml_utf8_encode(arg, arg_len, &len, "ISO-8859-1");
	if (encoded == NULL) {
		RETURN_FALSE;
	}
	RETVAL_STRINGL(encoded, len, 0);
}
/* }}} */

/* {{{ proto string utf8_decode(string data) 
   Converts a UTF-8 encoded string to ISO-8859-1 */
PHP_FUNCTION(utf8_decode_xml)
{
	char *arg;
	XML_Char *decoded;
	int arg_len, len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	decoded = xml_utf8_decode_old(arg, arg_len, &len, "ISO-8859-1");
	if (decoded == NULL) {
		RETURN_FALSE;
	}
	RETVAL_STRINGL(decoded, len, 0);
}
/* }}} */

/* {{{  */
PHP_FUNCTION(utf8_ord_new)
{
	char *arg;
	int len, status = 0;
	unsigned int value = 0;
	size_t cursor = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &len) == FAILURE) {
		return;
	}

	value = php_next_utf8_char(arg, len, &cursor, &status);

	RETVAL_LONG(value);
}
/* }}} */

/* {{{  */
PHP_FUNCTION(utf8_ord_old)
{
	char *arg;
	int len, status = 0;
	unsigned int value = 0;
	size_t cursor = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &len) == FAILURE) {
		return;
	}

	value = php_next_utf8_char_old(arg, len, &cursor, &status);

	RETVAL_LONG(value);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
