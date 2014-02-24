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
   | Authors: Rouven Weßling <me@rouvenwessling.de>                       |
   +----------------------------------------------------------------------+
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "zend_interfaces.h"
#include "ext/standard/php_rand.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

#include "php_spl.h"
#include "spl_iterators.h"
#include "spl_exceptions.h"
#include "spl_functions.h"

#define STATE_SIZE MT_N * (4 / sizeof(char))

#define N			 MT_N				 /* length of state vector */
#define M			 (397)				/* a period parameter */
#define hiBit(u)	  ((u) & 0x80000000U)  /* mask all but highest   bit of u */
#define loBit(u)	  ((u) & 0x00000001U)  /* mask all but lowest	bit of u */
#define loBits(u)	 ((u) & 0x7FFFFFFFU)  /* mask	 the highest   bit of u */
#define mixBits(u, v) (hiBit(u)|loBits(v)) /* move hi bit of u to hi bit of v */

#define twist(m,u,v)  (m ^ (mixBits(u,v)>>1) ^ ((php_uint32)(-(php_int32)(loBit(u))) & 0x9908b0dfU))

/* {{{ sq_initialize
 */
static void sq_initialize(php_uint32 seed, php_uint32 *state)
{
	/* Initialize generator state with seed
	   See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	   In previous versions, most significant bits (MSBs) of the seed affect
	   only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto. */

	register php_uint32 *s = state;
	register php_uint32 *r = state;
	register int i = 1;

	*s++ = seed & 0xffffffffU;
	for( ; i < N; ++i ) {
		*s++ = ( 1812433253U * ( *r ^ (*r >> 30) ) + i ) & 0xffffffffU;
		r++;
	}
}
/* }}} */

/* {{{ sq_reload
 */
static void sq_reload(php_uint32 state[], int *left TSRMLS_DC)
{
	/* Generate N new values in state
	   Made clearer and faster by Matthew Bellew (matthew.bellew@home.com) */

	register php_uint32 *p = state;
	register int i;

	for (i = N - M; i--; ++p)
		*p = twist(p[M], p[0], p[1]);
	for (i = M; --i; ++p)
		*p = twist(p[M-N], p[0], p[1]);
	*p = twist(p[M-N], p[0], state[0]);
	*left = N;
}
/* }}} */

/* {{{ sg_get_number
 */
static php_uint32 sq_get_number(php_uint32 state[], int *left TSRMLS_DC)
{
	/* Pull a 32-bit integer from the generator state
	   Every other access function simply transforms the numbers extracted here */
	
	register php_uint32 s1;

	if (*left == 0) {
		sq_reload(state, left TSRMLS_CC);
	}
	--*left;

	s1 = state[N - *left - 1];
	s1 ^= (s1 >> 11);
	s1 ^= (s1 <<  7) & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;
	return ( s1 ^ (s1 >> 18) );
}
/* }}} */

PHPAPI zend_class_entry	 *spl_ce_SplSequenceGenerator;

ZEND_BEGIN_ARG_INFO(arginfo_SplSequenceGenerator___construct, 0)
	ZEND_ARG_INFO(0, seed)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_SplSequenceGenerator_getNextNumber, 0, 0, 0)
	ZEND_ARG_INFO(0, min)
	ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO(arginfo_SplSequenceGenerator_serialize, 0)
	ZEND_ARG_INFO(0, seed)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO(arginfo_SplSequenceGenerator_unserialize, 0)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();

typedef struct _spl_sequencegenerator_object { /* {{{ */
	zend_object	std;
	php_uint32	 state[MT_N+1];  /* state vector + 1 extra to not violate ANSI C */
	int			left;		   /* can access this many values before reloading */
} spl_sequencegenerator_object;
/* }}} */

PHPAPI zend_object_handlers spl_handler_SplSequenceGenerator;

SPL_METHOD(SplSequenceGenerator, __construct)
{
	php_uint32 seed = 0;
	zval *object;
	spl_sequencegenerator_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &seed) == FAILURE) {
		return;
	}

	object = getThis();
	intern = zend_object_store_get_object(object TSRMLS_CC);

	if (ZEND_NUM_ARGS() == 0) {
		seed = GENERATE_SEED();
	}
	
	sq_initialize(seed, intern->state);
	sq_reload(&intern->state, &intern->left TSRMLS_CC);
}

SPL_METHOD(SplSequenceGenerator, serialize)
{
	spl_sequencegenerator_object *intern;
	smart_str buf = {0};
	php_serialize_data_t var_hash;
	zval zv, *zv_ptr = &zv;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = zend_object_store_get_object(getThis() TSRMLS_CC);
	if (!intern->state && !intern->left) {
		return;
	}

	PHP_VAR_SERIALIZE_INIT(var_hash);

	INIT_PZVAL(zv_ptr);

	ZVAL_LONG(zv_ptr, intern->left);
	php_var_serialize(&buf, &zv_ptr, &var_hash TSRMLS_CC);

	ZVAL_STRINGL(zv_ptr, (char *) intern->state, STATE_SIZE, 0);
	php_var_serialize(&buf, &zv_ptr, &var_hash TSRMLS_CC);

	Z_ARRVAL_P(zv_ptr) = zend_std_get_properties(getThis() TSRMLS_CC);
	Z_TYPE_P(zv_ptr) = IS_ARRAY;
	php_var_serialize(&buf, &zv_ptr, &var_hash TSRMLS_CC);

	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	if (buf.c) {
		RETURN_STRINGL(buf.c, buf.len, 0);
	}
}

SPL_METHOD(SplSequenceGenerator, unserialize)
{
	spl_sequencegenerator_object *intern;
	char *buf;
	int buf_len;
	php_unserialize_data_t var_hash;
	const unsigned char *p, *max;
	zval zv, *zv_ptr = &zv;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &buf, &buf_len) == FAILURE) {
		return;
	}

	if (buf_len == 0) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Empty serialized string cannot be empty");
		return;
	}

	intern = zend_object_store_get_object(getThis() TSRMLS_CC);

	if (intern->state && intern->left) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Cannot call unserialize() on an already constructed object");
		return;
	}

	PHP_VAR_UNSERIALIZE_INIT(var_hash);

	p = (unsigned char *) buf;
	max = (unsigned char *) buf + buf_len;

	INIT_ZVAL(zv);
	if (!php_var_unserialize(&zv_ptr, &p, max, &var_hash TSRMLS_CC)
		|| Z_TYPE_P(zv_ptr) != IS_LONG) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Error at offset %ld of %d bytes", (long)((char*)p - buf), buf_len);
		goto outexcept;
	}

	intern->left = Z_LVAL_P(zv_ptr);

	INIT_ZVAL(zv);
	if (!php_var_unserialize(&zv_ptr, &p, max, &var_hash TSRMLS_CC)
		|| Z_TYPE_P(zv_ptr) != IS_STRING || Z_STRLEN_P(zv_ptr) == 0) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Error at offset %ld of %d bytes", (long)((char*)p - buf), buf_len);
		goto outexcept;
	}
	if (STATE_SIZE != Z_STRLEN_P(zv_ptr)) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Expected state size %d, got %d", STATE_SIZE, Z_STRLEN_P(zv_ptr));
	}

	memcpy(intern->state, Z_STRVAL_P(zv_ptr), Z_STRLEN_P(zv_ptr));

	INIT_ZVAL(zv);
	if (!php_var_unserialize(&zv_ptr, &p, max, &var_hash TSRMLS_CC)
		|| Z_TYPE_P(zv_ptr) != IS_ARRAY) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Error at offset %ld of %d bytes", (long)((char*)p - buf), buf_len);
		goto outexcept;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(zv_ptr)) != 0) {
		zend_hash_copy(
			zend_std_get_properties(getThis() TSRMLS_CC), Z_ARRVAL_P(zv_ptr),
			(copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *)
		);
	}

outexcept:
	zval_dtor(zv_ptr);
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
	return;
}

SPL_METHOD(SplSequenceGenerator, getNextNumber)
{
	zval *object;
	spl_sequencegenerator_object *intern;

	object = getThis();
	intern = zend_object_store_get_object(object TSRMLS_CC);

	long min;
	long max;
	long number;
	int  argc = ZEND_NUM_ARGS();

	if (argc != 0) {
		if (zend_parse_parameters(argc TSRMLS_CC, "ll", &min, &max) == FAILURE) {
			return;
		} else if (max < min) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "max(%ld) is smaller than min(%ld)", max, min);
			RETURN_FALSE;
		}
	}

	number = (long) (sq_get_number(&intern->state, &intern->left TSRMLS_C) >> 1);
	if (argc == 2) {
		RAND_RANGE(number, min, max, PHP_MT_RAND_MAX);
	}

	RETURN_LONG(number);
}

static const zend_function_entry spl_funcs_SplSequenceGenerator[] = {
	SPL_ME(SplSequenceGenerator, __construct,	   arginfo_SplSequenceGenerator___construct,	ZEND_ACC_PUBLIC)
	SPL_ME(SplSequenceGenerator, serialize,	 	arginfo_SplSequenceGenerator_serialize,	  ZEND_ACC_PUBLIC)
	SPL_ME(SplSequenceGenerator, unserialize,	   arginfo_SplSequenceGenerator_unserialize,	ZEND_ACC_PUBLIC)
	SPL_ME(SplSequenceGenerator, getNextNumber,	 arginfo_SplSequenceGenerator_getNextNumber,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};

static void spl_sequencegenerator_object_free_storage(spl_sequencegenerator_object *intern TSRMLS_DC)
{
	zend_object_std_dtor(&intern->std TSRMLS_CC);
	efree(intern);
}
/* }}} */

static zend_object_value spl_sequencegenerator_new(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
	zend_object_value retval;

	spl_sequencegenerator_object *intern = ecalloc(1, sizeof(spl_sequencegenerator_object));

	zend_object_std_init(&intern->std, class_type TSRMLS_CC);
	object_properties_init(&intern->std, class_type);

	retval.handle = zend_objects_store_put(
		intern,
		(zend_objects_store_dtor_t) zend_objects_destroy_object,
		(zend_objects_free_object_storage_t) spl_sequencegenerator_object_free_storage,
		NULL TSRMLS_CC
	);

	retval.handlers = &spl_handler_SplSequenceGenerator;

	return retval;
}
/* }}} */

static zend_object_value spl_sequencegenerator_object_clone(zval *object TSRMLS_DC)
{
	spl_sequencegenerator_object *old_object = zend_object_store_get_object(object TSRMLS_CC);

	zend_object_value new_object_val = spl_sequencegenerator_new(Z_OBJCE_P(object) TSRMLS_CC);

	spl_sequencegenerator_object *new_object = zend_object_store_get_object_by_handle(
		new_object_val.handle TSRMLS_CC
	);

	zend_objects_clone_members(
		&new_object->std, new_object_val,
		&old_object->std, Z_OBJ_HANDLE_P(object) TSRMLS_CC
	);

	memcpy(new_object->state, old_object->state, sizeof(old_object->state));
	new_object->left = old_object->left;

	return new_object_val;
}

/* {{{ PHP_MINIT_FUNCTION(spl_sequence) */
PHP_MINIT_FUNCTION(spl_sequence)
{
	REGISTER_SPL_STD_CLASS_EX(SplSequenceGenerator, spl_sequencegenerator_new, spl_funcs_SplSequenceGenerator);
	REGISTER_SPL_IMPLEMENTS(SplSequenceGenerator, Serializable);
	memcpy(&spl_handler_SplSequenceGenerator, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	spl_handler_SplSequenceGenerator.clone_obj = spl_sequencegenerator_object_clone;

	REGISTER_SPL_CLASS_CONST_LONG(SplSequenceGenerator, "MAX", PHP_MT_RAND_MAX);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
