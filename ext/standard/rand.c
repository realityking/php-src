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
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   |          Zeev Suraski <zeev@zend.com>                                |
   |          Pedro Melo <melo@ip.pt>                                     |
   |          Sterling Hughes <sterling@php.net>                          |
   |                                                                      |
   | Based on code from: Richard J. Wagner <rjwagner@writeme.com>         |
   |                     Makoto Matsumoto <matumoto@math.keio.ac.jp>      |
   |                     Takuji Nishimura                                 |
   |                     Shawn Cokus <Cokus@math.washington.edu>          |
   +----------------------------------------------------------------------+
 */
/* $Id$ */

#include <stdlib.h>

#include "php.h"
#include "php_math.h"
#include "php_rand.h"
#include "php_lcg.h"

#include "basic_functions.h"
#include "mersenne.h"


/* SYSTEM RAND FUNCTIONS */

/* {{{ php_srand
 */
PHPAPI void php_srand(long seed TSRMLS_DC)
{
#ifdef ZTS
	BG(rand_seed) = (unsigned int) seed;
#else
# if defined(HAVE_SRANDOM)
	srandom((unsigned int) seed);
# elif defined(HAVE_SRAND48)
	srand48(seed);
# else
	srand((unsigned int) seed);
# endif
#endif

	/* Seed only once */
	BG(rand_is_seeded) = 1;
}
/* }}} */

/* {{{ php_rand
 */
PHPAPI long php_rand(TSRMLS_D)
{
	long ret;

	if (!BG(rand_is_seeded)) {
		php_srand(GENERATE_SEED() TSRMLS_CC);
	}

#ifdef ZTS
	ret = php_rand_r(&BG(rand_seed));
#else
# if defined(HAVE_RANDOM)
	ret = random();
# elif defined(HAVE_LRAND48)
	ret = lrand48();
# else
	ret = rand();
# endif
#endif

	return ret;
}
/* }}} */


/* MT RAND FUNCTIONS */

/* {{{ php_mt_srand
 */
PHPAPI void php_mt_srand(php_uint32 seed TSRMLS_DC)
{
	/* Seed the generator with a simple uint32 */
	php_mt_initialize(seed, BG(state));
	php_mt_reload(&BG(state), &BG(left) TSRMLS_CC);

	/* Seed only once */
	BG(mt_rand_is_seeded) = 1;
}
/* }}} */

/* {{{ php_mt_rand
 */
PHPAPI php_uint32 php_mt_rand(TSRMLS_D)
{
	return php_mt_get_number(&BG(state), &BG(left) TSRMLS_CC);
}
/* }}} */

/* {{{ proto void srand([int seed])
   Seeds random number generator */
PHP_FUNCTION(srand)
{
	long seed = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &seed) == FAILURE)
		return;

	if (ZEND_NUM_ARGS() == 0)
		seed = GENERATE_SEED();

	php_srand(seed TSRMLS_CC);
}
/* }}} */

/* {{{ proto void mt_srand([int seed])
   Seeds Mersenne Twister random number generator */
PHP_FUNCTION(mt_srand)
{
	long seed = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &seed) == FAILURE) 
		return;

	if (ZEND_NUM_ARGS() == 0)
		seed = GENERATE_SEED();

	php_mt_srand(seed TSRMLS_CC);
}
/* }}} */


/*
 * A bit of tricky math here.  We want to avoid using a modulus because
 * that simply tosses the high-order bits and might skew the distribution
 * of random values over the range.  Instead we map the range directly.
 *
 * We need to map the range from 0...M evenly to the range a...b
 * Let n = the random number and n' = the mapped random number
 *
 * Then we have: n' = a + n(b-a)/M
 *
 * We have a problem here in that only n==M will get mapped to b which
 # means the chances of getting b is much much less than getting any of
 # the other values in the range.  We can fix this by increasing our range
 # artifically and using:
 #
 #               n' = a + n(b-a+1)/M
 *
 # Now we only have a problem if n==M which would cause us to produce a
 # number of b+1 which would be bad.  So we bump M up by one to make sure
 # this will never happen, and the final algorithm looks like this:
 #
 #               n' = a + n(b-a+1)/(M+1) 
 *
 * -RL
 */    

/* {{{ proto int rand([int min, int max])
   Returns a random number */
PHP_FUNCTION(rand)
{
	long min;
	long max;
	long number;
	int  argc = ZEND_NUM_ARGS();

	if (argc != 0 && zend_parse_parameters(argc TSRMLS_CC, "ll", &min, &max) == FAILURE)
		return;

	number = php_rand(TSRMLS_C);
	if (argc == 2) {
		RAND_RANGE(number, min, max, PHP_RAND_MAX);
	}

	RETURN_LONG(number);
}
/* }}} */

/* {{{ proto int mt_rand([int min, int max])
   Returns a random number from Mersenne Twister */
PHP_FUNCTION(mt_rand)
{
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

	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}

	/*
	 * Melo: hmms.. randomMT() returns 32 random bits...
	 * Yet, the previous php_rand only returns 31 at most.
	 * So I put a right shift to loose the lsb. It *seems*
	 * better than clearing the msb. 
	 * Update: 
	 * I talked with Cokus via email and it won't ruin the algorithm
	 */
	number = (long) (php_mt_rand(TSRMLS_C) >> 1);
	if (argc == 2) {
		RAND_RANGE(number, min, max, PHP_MT_RAND_MAX);
	}

	RETURN_LONG(number);
}
/* }}} */

/* {{{ proto int getrandmax(void)
   Returns the maximum value a random number can have */
PHP_FUNCTION(getrandmax)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	RETURN_LONG(PHP_RAND_MAX);
}
/* }}} */

/* {{{ proto int mt_getrandmax(void)
   Returns the maximum value a random number from Mersenne Twister can have */
PHP_FUNCTION(mt_getrandmax)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	/*
	 * Melo: it could be 2^^32 but we only use 2^^31 to maintain
	 * compatibility with the previous php_rand
	 */
  	RETURN_LONG(PHP_MT_RAND_MAX); /* 2^^31 */
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
