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

ZEND_BEGIN_ARG_INFO_EX(arginfo_utf8_is_valid, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_utf8_encode, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_utf8_decode, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

/* {{{ utf8_functions[]
 *
 * Every user visible function must have an entry in utf8_functions[].
 */
const zend_function_entry utf8_functions[] = {
	PHP_FE(utf8_is_valid,       arginfo_utf8_is_valid)
	PHP_FE(utf8_encode,			arginfo_utf8_encode)
	PHP_FE(utf8_decode,			arginfo_utf8_decode)
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
