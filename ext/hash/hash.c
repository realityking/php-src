/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  |         Scott MacVicar <scottmac@php.net>                            |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include "php_hash.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"

static int php_hash_le_hash;
HashTable php_hash_hashtable;

#if (PHP_MAJOR_VERSION >= 5)
# define DEFAULT_CONTEXT FG(default_context)
#else
# define DEFAULT_CONTEXT NULL
#endif

/* Hash Registry Access */

PHP_HASH_API const php_hash_ops *php_hash_fetch_ops(const char *algo, int algo_len) /* {{{ */
{
	php_hash_ops *ops;
	char *lower = estrndup(algo, algo_len);

	zend_str_tolower(lower, algo_len);
	if (SUCCESS != zend_hash_find(&php_hash_hashtable, lower, algo_len + 1, (void*)&ops)) {
		ops = NULL;
	}
	efree(lower);

	return ops;
}
/* }}} */

PHP_HASH_API void php_hash_register_algo(const char *algo, const php_hash_ops *ops) /* {{{ */
{
	int algo_len = strlen(algo);
	char *lower = estrndup(algo, algo_len);
	
	zend_str_tolower(lower, algo_len);
	zend_hash_add(&php_hash_hashtable, lower, algo_len + 1, (void*)ops, sizeof(php_hash_ops), NULL);
	efree(lower);
}
/* }}} */

PHP_HASH_API int php_hash_copy(const void *ops, void *orig_context, void *dest_context) /* {{{ */
{
	php_hash_ops *hash_ops = (php_hash_ops *)ops;

	memcpy(dest_context, orig_context, hash_ops->context_size);
	return SUCCESS;
}
/* }}} */

/* Userspace */

static void php_hash_do_hash(INTERNAL_FUNCTION_PARAMETERS, int isfilename, zend_bool raw_output_default) /* {{{ */
{
	char *algo, *data, *digest;
	int algo_len, data_len;
	zend_bool raw_output = raw_output_default;
	const php_hash_ops *ops;
	void *context;
	php_stream *stream = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b", &algo, &algo_len, &data, &data_len, &raw_output) == FAILURE) {
		return;
	}

	ops = php_hash_fetch_ops(algo, algo_len);
	if (!ops) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown hashing algorithm: %s", algo);
		RETURN_FALSE;
	}
	if (isfilename) {
		if (CHECK_NULL_PATH(data, data_len)) {
			RETURN_FALSE;
		}
		stream = php_stream_open_wrapper_ex(data, "rb", REPORT_ERRORS, NULL, DEFAULT_CONTEXT);
		if (!stream) {
			/* Stream will report errors opening file */
			RETURN_FALSE;
		}
	}

	context = emalloc(ops->context_size);
	ops->hash_init(context);

	if (isfilename) {
		char buf[1024];
		int n;

		while ((n = php_stream_read(stream, buf, sizeof(buf))) > 0) {
			ops->hash_update(context, (unsigned char *) buf, n);
		}
		php_stream_close(stream);
	} else {
		ops->hash_update(context, (unsigned char *) data, data_len);
	}

	digest = emalloc(ops->digest_size + 1);
	ops->hash_final((unsigned char *) digest, context);
	efree(context);

	if (raw_output) {
		digest[ops->digest_size] = 0;
		RETURN_STRINGL(digest, ops->digest_size, 0);
	} else {
		char *hex_digest = safe_emalloc(ops->digest_size, 2, 1);

		php_hash_bin2hex(hex_digest, (unsigned char *) digest, ops->digest_size);
		hex_digest[2 * ops->digest_size] = 0;
		efree(digest);
		RETURN_STRINGL(hex_digest, 2 * ops->digest_size, 0);
	}
}
/* }}} */

/* {{{ proto string hash(string algo, string data[, bool raw_output = false])
Generate a hash of a given input string
Returns lowercase hexits by default */
PHP_FUNCTION(hash)
{
	php_hash_do_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ proto string hash_file(string algo, string filename[, bool raw_output = false])
Generate a hash of a given file
Returns lowercase hexits by default */
PHP_FUNCTION(hash_file)
{
	php_hash_do_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

static inline void php_hash_string_xor_char(unsigned char *out, const unsigned char *in, const unsigned char xor_with, const int length) {
	int i;
	for (i=0; i < length; i++) {
		out[i] = in[i] ^ xor_with;
	}
}

static inline void php_hash_string_xor(unsigned char *out, const unsigned char *in, const unsigned char *xor_with, const int length) {
	int i;
	for (i=0; i < length; i++) {
		out[i] = in[i] ^ xor_with[i];
	}
}

static inline void php_hash_hmac_prep_key(unsigned char *K, const php_hash_ops *ops, void *context, const unsigned char *key, const int key_len) {
	memset(K, 0, ops->block_size);
	if (key_len > ops->block_size) {
		/* Reduce the key first */
		ops->hash_init(context);
		ops->hash_update(context, key, key_len);
		ops->hash_final(K, context);
	} else {
		memcpy(K, key, key_len);
	}
	/* XOR the key with 0x36 to get the ipad) */
	php_hash_string_xor_char(K, K, 0x36, ops->block_size);
}

static inline void php_hash_hmac_round(unsigned char *final, const php_hash_ops *ops, void *context, const unsigned char *key, const unsigned char *data, const long data_size) {
	ops->hash_init(context);
	ops->hash_update(context, key, ops->block_size);
	ops->hash_update(context, data, data_size);
	ops->hash_final(final, context);
}

static void php_hash_do_hash_hmac(INTERNAL_FUNCTION_PARAMETERS, int isfilename, zend_bool raw_output_default) /* {{{ */
{
	char *algo, *data, *digest, *key, *K;
	int algo_len, data_len, key_len;
	zend_bool raw_output = raw_output_default;
	const php_hash_ops *ops;
	void *context;
	php_stream *stream = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|b", &algo, &algo_len, &data, &data_len, 
																  &key, &key_len, &raw_output) == FAILURE) {
		return;
	}

	ops = php_hash_fetch_ops(algo, algo_len);
	if (!ops) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown hashing algorithm: %s", algo);
		RETURN_FALSE;
	}
	if (isfilename) {
		stream = php_stream_open_wrapper_ex(data, "rb", REPORT_ERRORS, NULL, DEFAULT_CONTEXT);
		if (!stream) {
			/* Stream will report errors opening file */
			RETURN_FALSE;
		}
	}

	context = emalloc(ops->context_size);

	K = emalloc(ops->block_size);
	digest = emalloc(ops->digest_size + 1);

	php_hash_hmac_prep_key((unsigned char *) K, ops, context, (unsigned char *) key, key_len);		

	if (isfilename) {
		char buf[1024];
		int n;
		ops->hash_init(context);
		ops->hash_update(context, (unsigned char *) K, ops->block_size);
		while ((n = php_stream_read(stream, buf, sizeof(buf))) > 0) {
			ops->hash_update(context, (unsigned char *) buf, n);
		}
		php_stream_close(stream);
		ops->hash_final((unsigned char *) digest, context);
	} else {
		php_hash_hmac_round((unsigned char *) digest, ops, context, (unsigned char *) K, (unsigned char *) data, data_len);
	}

	php_hash_string_xor_char((unsigned char *) K, (unsigned char *) K, 0x6A, ops->block_size);

	php_hash_hmac_round((unsigned char *) digest, ops, context, (unsigned char *) K, (unsigned char *) digest, ops->digest_size);

	/* Zero the key */
	memset(K, 0, ops->block_size);
	efree(K);
	efree(context);

	if (raw_output) {
		digest[ops->digest_size] = 0;
		RETURN_STRINGL(digest, ops->digest_size, 0);
	} else {
		char *hex_digest = safe_emalloc(ops->digest_size, 2, 1);

		php_hash_bin2hex(hex_digest, (unsigned char *) digest, ops->digest_size);
		hex_digest[2 * ops->digest_size] = 0;
		efree(digest);
		RETURN_STRINGL(hex_digest, 2 * ops->digest_size, 0);
	}
}
/* }}} */

/* {{{ proto string hash_hmac(string algo, string data, string key[, bool raw_output = false])
Generate a hash of a given input string with a key using HMAC
Returns lowercase hexits by default */
PHP_FUNCTION(hash_hmac)
{
	php_hash_do_hash_hmac(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ proto string hash_hmac_file(string algo, string filename, string key[, bool raw_output = false])
Generate a hash of a given file with a key using HMAC
Returns lowercase hexits by default */
PHP_FUNCTION(hash_hmac_file)
{
	php_hash_do_hash_hmac(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */


/* {{{ proto resource hash_init(string algo[, int options, string key])
Initialize a hashing context */
PHP_FUNCTION(hash_init)
{
	char *algo, *key = NULL;
	int algo_len, key_len = 0, argc = ZEND_NUM_ARGS();
	long options = 0;
	void *context;
	const php_hash_ops *ops;
	php_hash_data *hash;

	if (zend_parse_parameters(argc TSRMLS_CC, "s|ls", &algo, &algo_len, &options, &key, &key_len) == FAILURE) {
		return;
	}

	ops = php_hash_fetch_ops(algo, algo_len);
	if (!ops) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown hashing algorithm: %s", algo);
		RETURN_FALSE;
	}

	if (options & PHP_HASH_HMAC &&
		key_len <= 0) {
		/* Note: a zero length key is no key at all */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "HMAC requested without a key");
		RETURN_FALSE;
	}

	context = emalloc(ops->context_size);
	ops->hash_init(context);

	hash = emalloc(sizeof(php_hash_data));
	hash->ops = ops;
	hash->context = context;
	hash->options = options;
	hash->key = NULL;

	if (options & PHP_HASH_HMAC) {
		char *K = emalloc(ops->block_size);
		int i;

		memset(K, 0, ops->block_size);

		if (key_len > ops->block_size) {
			/* Reduce the key first */
			ops->hash_update(context, (unsigned char *) key, key_len);
			ops->hash_final((unsigned char *) K, context);
			/* Make the context ready to start over */
			ops->hash_init(context);
		} else {
			memcpy(K, key, key_len);
		}
			
		/* XOR ipad */
		for(i=0; i < ops->block_size; i++) {
			K[i] ^= 0x36;
		}
		ops->hash_update(context, (unsigned char *) K, ops->block_size);
		hash->key = (unsigned char *) K;
	}

	ZEND_REGISTER_RESOURCE(return_value, hash, php_hash_le_hash);
}
/* }}} */

/* {{{ proto bool hash_update(resource context, string data)
Pump data into the hashing algorithm */
PHP_FUNCTION(hash_update)
{
	zval *zhash;
	php_hash_data *hash;
	char *data;
	int data_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zhash, &data, &data_len) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(hash, php_hash_data*, &zhash, -1, PHP_HASH_RESNAME, php_hash_le_hash);

	hash->ops->hash_update(hash->context, (unsigned char *) data, data_len);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int hash_update_stream(resource context, resource handle[, integer length])
Pump data into the hashing algorithm from an open stream */
PHP_FUNCTION(hash_update_stream)
{
	zval *zhash, *zstream;
	php_hash_data *hash;
	php_stream *stream = NULL;
	long length = -1, didread = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rr|l", &zhash, &zstream, &length) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(hash, php_hash_data*, &zhash, -1, PHP_HASH_RESNAME, php_hash_le_hash);
	php_stream_from_zval(stream, &zstream);

	while (length) {
		char buf[1024];
		long n, toread = 1024;

		if (length > 0 && toread > length) {
			toread = length;
		}

		if ((n = php_stream_read(stream, buf, toread)) <= 0) {
			/* Nada mas */
			RETURN_LONG(didread);
		}
		hash->ops->hash_update(hash->context, (unsigned char *) buf, n);
		length -= n;
		didread += n;
	} 

	RETURN_LONG(didread);
}
/* }}} */

/* {{{ proto bool hash_update_file(resource context, string filename[, resource context])
Pump data into the hashing algorithm from a file */
PHP_FUNCTION(hash_update_file)
{
	zval *zhash, *zcontext = NULL;
	php_hash_data *hash;
	php_stream_context *context;
	php_stream *stream;
	char *filename, buf[1024];
	int filename_len, n;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|r", &zhash, &filename, &filename_len, &zcontext) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(hash, php_hash_data*, &zhash, -1, PHP_HASH_RESNAME, php_hash_le_hash);
	context = php_stream_context_from_zval(zcontext, 0);

	stream = php_stream_open_wrapper_ex(filename, "rb", REPORT_ERRORS, NULL, context);
	if (!stream) {
		/* Stream will report errors opening file */
		RETURN_FALSE;
	}

	while ((n = php_stream_read(stream, buf, sizeof(buf))) > 0) {
		hash->ops->hash_update(hash->context, (unsigned char *) buf, n);
	}
	php_stream_close(stream);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string hash_final(resource context[, bool raw_output=false])
Output resulting digest */
PHP_FUNCTION(hash_final)
{
	zval *zhash;
	php_hash_data *hash;
	zend_bool raw_output = 0;
	zend_rsrc_list_entry *le;
	char *digest;
	int digest_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|b", &zhash, &raw_output) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(hash, php_hash_data*, &zhash, -1, PHP_HASH_RESNAME, php_hash_le_hash);

	digest_len = hash->ops->digest_size;
	digest = emalloc(digest_len + 1);
	hash->ops->hash_final((unsigned char *) digest, hash->context);
	if (hash->options & PHP_HASH_HMAC) {
		int i;

		/* Convert K to opad -- 0x6A = 0x36 ^ 0x5C */
		for(i=0; i < hash->ops->block_size; i++) {
			hash->key[i] ^= 0x6A;
		}

		/* Feed this result into the outter hash */
		hash->ops->hash_init(hash->context);
		hash->ops->hash_update(hash->context, (unsigned char *) hash->key, hash->ops->block_size);
		hash->ops->hash_update(hash->context, (unsigned char *) digest, hash->ops->digest_size);
		hash->ops->hash_final((unsigned char *) digest, hash->context);

		/* Zero the key */
		memset(hash->key, 0, hash->ops->block_size);
		efree(hash->key);
		hash->key = NULL;
	}
	digest[digest_len] = 0;
	efree(hash->context);
	hash->context = NULL;

	/* zend_list_REAL_delete() */
	if (zend_hash_index_find(&EG(regular_list), Z_RESVAL_P(zhash), (void *) &le)==SUCCESS) {
		/* This is a hack to avoid letting the resource hide elsewhere (like in separated vars)
			FETCH_RESOURCE is intelligent enough to handle dealing with any issues this causes */
		le->refcount = 1;
	} /* FAILURE is not an option */
	zend_list_delete(Z_RESVAL_P(zhash));

	if (raw_output) {
		RETURN_STRINGL(digest, digest_len, 0);
	} else {
		char *hex_digest = safe_emalloc(digest_len,2,1);

		php_hash_bin2hex(hex_digest, (unsigned char *) digest, digest_len);
		hex_digest[2 * digest_len] = 0;
		efree(digest);
		RETURN_STRINGL(hex_digest, 2 * digest_len, 0);		
	}
}
/* }}} */

/* {{{ proto resource hash_copy(resource context)
Copy hash resource */
PHP_FUNCTION(hash_copy)
{
	zval *zhash;
	php_hash_data *hash, *copy_hash;
	void *context;
	int res;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zhash) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(hash, php_hash_data*, &zhash, -1, PHP_HASH_RESNAME, php_hash_le_hash);


	context = emalloc(hash->ops->context_size);
	hash->ops->hash_init(context);

	res = hash->ops->hash_copy(hash->ops, hash->context, context);
	if (res != SUCCESS) {
		efree(context);
		RETURN_FALSE;
	}

	copy_hash = emalloc(sizeof(php_hash_data));
	copy_hash->ops = hash->ops;
	copy_hash->context = context;
	copy_hash->options = hash->options;
	copy_hash->key = ecalloc(1, hash->ops->block_size);
	if (hash->key) {
		memcpy(copy_hash->key, hash->key, hash->ops->block_size);
	}
	ZEND_REGISTER_RESOURCE(return_value, copy_hash, php_hash_le_hash);
}
/* }}} */

/* {{{ proto array hash_algos(void)
Return a list of registered hashing algorithms */
PHP_FUNCTION(hash_algos)
{
	HashPosition pos;
	char *str;
	uint str_len;
	long type;
	ulong idx;

	array_init(return_value);
	for(zend_hash_internal_pointer_reset_ex(&php_hash_hashtable, &pos);
		(type = zend_hash_get_current_key_ex(&php_hash_hashtable, &str, &str_len, &idx, 0, &pos)) != HASH_KEY_NON_EXISTENT;
		zend_hash_move_forward_ex(&php_hash_hashtable, &pos)) {
		add_next_index_stringl(return_value, str, str_len-1, 1);
	}
}
/* }}} */

/* {{{ proto string hash_pbkdf2(string algo, string password, string salt, int iterations [, int length = 0, bool raw_output = false])
Generate a PBKDF2 hash of the given password and salt
Returns lowercase hexits by default */
PHP_FUNCTION(hash_pbkdf2)
{
	char *returnval, *algo, *salt, *pass;
	unsigned char *computed_salt, *digest, *temp, *result, *K1, *K2;
	long loops, i, j, iterations, length = 0, digest_length;
	int algo_len, pass_len, salt_len;
	zend_bool raw_output = 0;
	const php_hash_ops *ops;
	void *context;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssl|lb", &algo, &algo_len, &pass, &pass_len, &salt, &salt_len, &iterations, &length, &raw_output) == FAILURE) {
		return;
	}

	ops = php_hash_fetch_ops(algo, algo_len);
	if (!ops) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown hashing algorithm: %s", algo);
		RETURN_FALSE;
	}

	if (iterations <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Iterations must be a positive integer: %ld", iterations);
		RETURN_FALSE;
	}

	if (length < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Length must be greater than or equal to 0: %ld", length);
		RETURN_FALSE;
	}

	if (salt_len > INT_MAX - 4) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Supplied salt is too long, max of INT_MAX - 4 bytes: %d supplied", salt_len);
		RETURN_FALSE;
	}

	context = emalloc(ops->context_size);
	ops->hash_init(context);

	K1 = emalloc(ops->block_size);
	K2 = emalloc(ops->block_size);
	digest = emalloc(ops->digest_size);
	temp = emalloc(ops->digest_size);

	/* Setup Keys that will be used for all hmac rounds */
	php_hash_hmac_prep_key(K1, ops, context, (unsigned char *) pass, pass_len);
	/* Convert K1 to opad -- 0x6A = 0x36 ^ 0x5C */
	php_hash_string_xor_char(K2, K1, 0x6A, ops->block_size);

	/* Setup Main Loop to build a long enough result */
	if (length == 0) {
		length = ops->digest_size;
		if (!raw_output) {
			length = length * 2;
		}
	}
	digest_length = length;
	if (!raw_output) {
		digest_length = (long) ceil((float) length / 2.0);
	}

	loops = (long) ceil((float) digest_length / (float) ops->digest_size);

	result = safe_emalloc(loops, ops->digest_size, 0);

	computed_salt = safe_emalloc(salt_len, 1, 4);
	memcpy(computed_salt, (unsigned char *) salt, salt_len);

	for (i = 1; i <= loops; i++) {
		/* digest = hash_hmac(salt + pack('N', i), password) { */

		/* pack("N", i) */
		computed_salt[salt_len] = (unsigned char) (i >> 24);
		computed_salt[salt_len + 1] = (unsigned char) ((i & 0xFF0000) >> 16);
		computed_salt[salt_len + 2] = (unsigned char) ((i & 0xFF00) >> 8);
		computed_salt[salt_len + 3] = (unsigned char) (i & 0xFF);

		php_hash_hmac_round(digest, ops, context, K1, computed_salt, (long) salt_len + 4);
		php_hash_hmac_round(digest, ops, context, K2, digest, ops->digest_size);
		/* } */

		/* temp = digest */
		memcpy(temp, digest, ops->digest_size);

		/* 
		 * Note that the loop starting at 1 is intentional, since we've already done
		 * the first round of the algorithm.
		 */
		for (j = 1; j < iterations; j++) {
			/* digest = hash_hmac(digest, password) { */
			php_hash_hmac_round(digest, ops, context, K1, digest, ops->digest_size);
			php_hash_hmac_round(digest, ops, context, K2, digest, ops->digest_size);
			/* } */
			/* temp ^= digest */
			php_hash_string_xor(temp, temp, digest, ops->digest_size);
		}
		/* result += temp */
		memcpy(result + ((i - 1) * ops->digest_size), temp, ops->digest_size);
	}
	/* Zero potentially sensitive variables */
	memset(K1, 0, ops->block_size);
	memset(K2, 0, ops->block_size);
	memset(computed_salt, 0, salt_len + 4);
	efree(K1);
	efree(K2);
	efree(computed_salt);
	efree(context);
	efree(digest);
	efree(temp);

	returnval = safe_emalloc(length, 1, 1);
	if (raw_output) {
		memcpy(returnval, result, length);
	} else {
		php_hash_bin2hex(returnval, result, digest_length);
	}
	returnval[length] = 0;
	efree(result);
	RETURN_STRINGL(returnval, length, 0);
}
/* }}} */

/* {{{ proto bool hash_equals(string known_string, string user_string)
   Compares two strings using the same time whether they're equal or not.
   A difference in length will leak */
PHP_FUNCTION(hash_equals)
{
	zval *known_zval, *user_zval;
	char *known_str, *user_str;
	int result = 0, j;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &known_zval, &user_zval) == FAILURE) {
		return;
	}

	/* We only allow comparing string to prevent unexpected results. */
	if (Z_TYPE_P(known_zval) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expected known_string to be a string, %s given", zend_zval_type_name(known_zval));
		RETURN_FALSE;
	}

	if (Z_TYPE_P(user_zval) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expected user_string to be a string, %s given", zend_zval_type_name(user_zval));
		RETURN_FALSE;
	}

	if (Z_STRLEN_P(known_zval) != Z_STRLEN_P(user_zval)) {
		RETURN_FALSE;
	}

	known_str = Z_STRVAL_P(known_zval);
	user_str = Z_STRVAL_P(user_zval);

	/* This is security sensitive code. Do not optimize this for speed. */
	for (j = 0; j < Z_STRLEN_P(known_zval); j++) {
		result |= known_str[j] ^ user_str[j];
	}

	RETURN_BOOL(0 == result);
}
/* }}} */

/* Module Housekeeping */

static void php_hash_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	php_hash_data *hash = (php_hash_data*)rsrc->ptr;

	/* Just in case the algo has internally allocated resources */
	if (hash->context) {
		unsigned char *dummy = emalloc(hash->ops->digest_size);
		hash->ops->hash_final(dummy, hash->context);
		efree(dummy);
		efree(hash->context);
	}

	if (hash->key) {
		memset(hash->key, 0, hash->ops->block_size);
		efree(hash->key);
	}
	efree(hash);
}
/* }}} */

#define PHP_HASH_HAVAL_REGISTER(p,b)	php_hash_register_algo("haval" #b "," #p , &php_hash_##p##haval##b##_ops);

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(hash)
{
	php_hash_le_hash = zend_register_list_destructors_ex(php_hash_dtor, NULL, PHP_HASH_RESNAME, module_number);

	zend_hash_init(&php_hash_hashtable, 35, NULL, NULL, 1);

	php_hash_register_algo("md2",			&php_hash_md2_ops);
	php_hash_register_algo("md4",			&php_hash_md4_ops);
	php_hash_register_algo("md5",			&php_hash_md5_ops);
	php_hash_register_algo("sha1",			&php_hash_sha1_ops);
	php_hash_register_algo("sha224",		&php_hash_sha224_ops);
	php_hash_register_algo("sha256",		&php_hash_sha256_ops);
	php_hash_register_algo("sha384",		&php_hash_sha384_ops);
	php_hash_register_algo("sha512",		&php_hash_sha512_ops);
	php_hash_register_algo("ripemd128",		&php_hash_ripemd128_ops);
	php_hash_register_algo("ripemd160",		&php_hash_ripemd160_ops);
	php_hash_register_algo("ripemd256",		&php_hash_ripemd256_ops);
	php_hash_register_algo("ripemd320",		&php_hash_ripemd320_ops);
	php_hash_register_algo("whirlpool",		&php_hash_whirlpool_ops);
	php_hash_register_algo("tiger128,3",	&php_hash_3tiger128_ops);
	php_hash_register_algo("tiger160,3",	&php_hash_3tiger160_ops);
	php_hash_register_algo("tiger192,3",	&php_hash_3tiger192_ops);
	php_hash_register_algo("tiger128,4",	&php_hash_4tiger128_ops);
	php_hash_register_algo("tiger160,4",	&php_hash_4tiger160_ops);
	php_hash_register_algo("tiger192,4",	&php_hash_4tiger192_ops);
	php_hash_register_algo("snefru",		&php_hash_snefru_ops);
	php_hash_register_algo("snefru256",		&php_hash_snefru_ops);
	php_hash_register_algo("gost",			&php_hash_gost_ops);
	php_hash_register_algo("gost-crypto",		&php_hash_gost_crypto_ops);
	php_hash_register_algo("adler32",		&php_hash_adler32_ops);
	php_hash_register_algo("crc32",			&php_hash_crc32_ops);
	php_hash_register_algo("crc32b",		&php_hash_crc32b_ops);
	php_hash_register_algo("fnv132",		&php_hash_fnv132_ops);
	php_hash_register_algo("fnv1a32",		&php_hash_fnv1a32_ops);
	php_hash_register_algo("fnv164",		&php_hash_fnv164_ops);
	php_hash_register_algo("fnv1a64",		&php_hash_fnv1a64_ops);
	php_hash_register_algo("joaat",			&php_hash_joaat_ops);

	PHP_HASH_HAVAL_REGISTER(3,128);
	PHP_HASH_HAVAL_REGISTER(3,160);
	PHP_HASH_HAVAL_REGISTER(3,192);
	PHP_HASH_HAVAL_REGISTER(3,224);
	PHP_HASH_HAVAL_REGISTER(3,256);

	PHP_HASH_HAVAL_REGISTER(4,128);
	PHP_HASH_HAVAL_REGISTER(4,160);
	PHP_HASH_HAVAL_REGISTER(4,192);
	PHP_HASH_HAVAL_REGISTER(4,224);
	PHP_HASH_HAVAL_REGISTER(4,256);

	PHP_HASH_HAVAL_REGISTER(5,128);
	PHP_HASH_HAVAL_REGISTER(5,160);
	PHP_HASH_HAVAL_REGISTER(5,192);
	PHP_HASH_HAVAL_REGISTER(5,224);
	PHP_HASH_HAVAL_REGISTER(5,256);

	REGISTER_LONG_CONSTANT("HASH_HMAC",		PHP_HASH_HMAC,	CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(hash)
{
	zend_hash_destroy(&php_hash_hashtable);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(hash)
{
	HashPosition pos;
	char buffer[2048];
	char *s = buffer, *e = s + sizeof(buffer), *str;
	ulong idx;
	long type;

	for(zend_hash_internal_pointer_reset_ex(&php_hash_hashtable, &pos);
		(type = zend_hash_get_current_key_ex(&php_hash_hashtable, &str, NULL, &idx, 0, &pos)) != HASH_KEY_NON_EXISTENT;
		zend_hash_move_forward_ex(&php_hash_hashtable, &pos)) {
		s += slprintf(s, e - s, "%s ", str);
	}
	*s = 0;

	php_info_print_table_start();
	php_info_print_table_row(2, "hash support", "enabled");
	php_info_print_table_row(2, "Hashing Engines", buffer);
	php_info_print_table_end();
}
/* }}} */

/* {{{ arginfo */
#ifdef PHP_HASH_MD5_NOT_IN_CORE
ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_md5, 0, 0, 1)
	ZEND_ARG_INFO(0, str)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_md5_file, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()
#endif

#ifdef PHP_HASH_SHA1_NOT_IN_CORE
ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_sha1, 0, 0, 1)
	ZEND_ARG_INFO(0, str)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_sha1_file, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash, 0, 0, 2)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_file, 0, 0, 2)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_hmac, 0, 0, 3)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_hmac_file, 0, 0, 3)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_init, 0, 0, 1)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, options)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_hash_update, 0)
	ZEND_ARG_INFO(0, context)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_update_stream, 0, 0, 2)
	ZEND_ARG_INFO(0, context)
	ZEND_ARG_INFO(0, handle)
	ZEND_ARG_INFO(0, length)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_update_file, 0, 0, 2)
	ZEND_ARG_INFO(0, context)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, context)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_final, 0, 0, 1)
	ZEND_ARG_INFO(0, context)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_hash_copy, 0)
	ZEND_ARG_INFO(0, context)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_hash_algos, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_pbkdf2, 0, 0, 4)
	ZEND_ARG_INFO(0, algo)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, salt)
	ZEND_ARG_INFO(0, iterations)
	ZEND_ARG_INFO(0, length)
	ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_hash_equals, 0)
	ZEND_ARG_INFO(0, known_string)
	ZEND_ARG_INFO(0, user_string)
ZEND_END_ARG_INFO()

/* }}} */

/* {{{ hash_functions[]
 */
const zend_function_entry hash_functions[] = {
	PHP_FE(hash,									arginfo_hash)
	PHP_FE(hash_file,								arginfo_hash_file)

	PHP_FE(hash_hmac,								arginfo_hash_hmac)
	PHP_FE(hash_hmac_file,							arginfo_hash_hmac_file)

	PHP_FE(hash_init,								arginfo_hash_init)
	PHP_FE(hash_update,								arginfo_hash_update)
	PHP_FE(hash_update_stream,						arginfo_hash_update_stream)
	PHP_FE(hash_update_file,						arginfo_hash_update_file)
	PHP_FE(hash_final,								arginfo_hash_final)
	PHP_FE(hash_copy,								arginfo_hash_copy)

	PHP_FE(hash_algos,								arginfo_hash_algos)
	PHP_FE(hash_pbkdf2,								arginfo_hash_pbkdf2)
	PHP_FE(hash_equals,								arginfo_hash_equals)

	/* BC Land */
#ifdef PHP_HASH_MD5_NOT_IN_CORE
	PHP_NAMED_FE(md5, php_if_md5,					arginfo_hash_md5)
	PHP_NAMED_FE(md5_file, php_if_md5_file,			arginfo_hash_md5_file)
#endif /* PHP_HASH_MD5_NOT_IN_CORE */

#ifdef PHP_HASH_SHA1_NOT_IN_CORE
	PHP_NAMED_FE(sha1, php_if_sha1,					arginfo_hash_sha1)
	PHP_NAMED_FE(sha1_file, php_if_sha1_file,		arginfo_hash_sha1_file)
#endif /* PHP_HASH_SHA1_NOT_IN_CORE */

	PHP_FE_END
};
/* }}} */

/* {{{ hash_module_entry
 */
zend_module_entry hash_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_HASH_EXTNAME,
	hash_functions,
	PHP_MINIT(hash),
	PHP_MSHUTDOWN(hash),
	NULL, /* RINIT */
	NULL, /* RSHUTDOWN */
	PHP_MINFO(hash),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_HASH_EXTVER, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HASH
ZEND_GET_MODULE(hash)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

