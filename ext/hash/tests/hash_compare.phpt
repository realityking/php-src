--TEST--
hash_compare() function
--FILE--
<?php
var_dump(hash_compare("same", "same"));
var_dump(hash_compare("not1same", "not2same"));
var_dump(hash_compare("short", "longer"));
var_dump(hash_compare("longer", "short"));
var_dump(hash_compare("", "notempty"));
var_dump(hash_compare("notempty", ""));
var_dump(hash_compare("", ""));
var_dump(hash_compare(123, "NaN"));
var_dump(hash_compare("NaN", 123));
var_dump(hash_compare(123, 123));
var_dump(hash_compare(null, ""));
var_dump(hash_compare(null, 123));
var_dump(hash_compare(null, null));
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
