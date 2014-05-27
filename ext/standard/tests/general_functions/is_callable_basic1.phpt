--TEST--
Test is_callable() function : usage variations - defined functions
--INI--
precision=14
error_reporting = E_ALL & ~E_NOTICE | E_STRICT
--FILE--
<?php
/* Prototype: bool is_callable ( mixed $var [, bool $syntax_only [, string &$callable_name]] );
 * Description: Verify that the contents of a variable can be called as a function
 * Source code: ext/imap/php_imap.c
 */

/* Prototype: void check_iscallable( $functions );
   Description: use iscallable() on given string to check for valid function name
                returns true if valid function name, false otherwise
*/
function check_iscallable( $functions ) {
  $counter = 1;
  foreach($functions as $func) {
    echo "-- Iteration  $counter --\n";
    var_dump( is_callable($func) );  //given only $var argument
    var_dump( is_callable($func, TRUE) );  //given $var and $syntax argument
    var_dump( is_callable($func, TRUE, $callable_name) );
    echo $callable_name, "\n";
    var_dump( is_callable($func, FALSE) );  //given $var and $syntax argument
    var_dump( is_callable($func, FALSE, $callable_name) );
    echo $callable_name, "\n";
    $counter++;
  }
}

echo "\n*** Testing is_callable() on defined functions ***\n";
/* function name with simple string */
function someFunction() {
}

/* function name with mixed string and integer */
function x123() {
}

/* function name with string and special character */
function Hello_World() {
}

$defined_functions = array (
  $functionVar1 = 'someFunction',
  $functionVar2 = 'x123',
  $functionVar5 = "Hello_World"
);
/* use check_iscallable() to check whether given string is valid function name
 *  expected: true as it is valid callback
 */
check_iscallable($defined_functions);

?>
===DONE===
--EXPECT--
*** Testing is_callable() on defined functions ***
-- Iteration  1 --
bool(true)
bool(true)
bool(true)
someFunction
bool(true)
bool(true)
someFunction
-- Iteration  2 --
bool(true)
bool(true)
bool(true)
x123
bool(true)
bool(true)
x123
-- Iteration  3 --
bool(true)
bool(true)
bool(true)
Hello_World
bool(true)
bool(true)
Hello_World
===DONE===
