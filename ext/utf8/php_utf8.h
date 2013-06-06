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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_UTF8_H
#define PHP_UTF8_H

extern zend_module_entry utf8_module_entry;
#define phpext_utf8_ptr &utf8_module_entry

#ifdef PHP_WIN32
#	define PHP_UTF8_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_UTF8_API __attribute__ ((visibility("default")))
#else
#	define PHP_UTF8_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(utf8);
PHP_MSHUTDOWN_FUNCTION(utf8);
PHP_MINFO_FUNCTION(utf8);

PHP_FUNCTION(utf8_is_valid);
PHP_FUNCTION(utf8_encode);
PHP_FUNCTION(utf8_decode);

PHP_FUNCTION(utf8_is_valid_json);
PHP_FUNCTION(utf8_to_utf16_json);
PHP_FUNCTION(utf8_to_utf16_intl);
PHP_FUNCTION(utf8_to_utf16);
PHP_FUNCTION(utf8_encode_xml);
PHP_FUNCTION(utf8_decode_xml);
PHP_FUNCTION(utf8_ord_old);
PHP_FUNCTION(utf8_ord_new);

#ifdef ZTS
#define UTF8_G(v) TSRMG(utf8_globals_id, zend_utf8_globals *, v)
#else
#define UTF8_G(v) (utf8_globals.v)
#endif

#endif	/* PHP_UTF8_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
