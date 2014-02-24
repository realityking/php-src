<?php
echo "Max\n";
var_dump(SplSequenceGenerator::MAX);
var_dump(mt_getrandmax());

$obj = new SplSequenceGenerator(20);
mt_srand(20);

for ($i = 0; $i < 1000; $i++)
{
	echo "Itr $i\n";
	var_dump($obj->getNextNumber(0, 1000));
	var_dump(mt_rand(0, 1000));
}
$b = clone $obj;
$c = unserialize(serialize($b));
echo "End\n";

var_dump($obj->getNextNumber(0, 1000));
var_dump($b->getNextNumber(0, 1000));
var_dump($c->getNextNumber(0, 1000));
var_dump(mt_rand(0, 1000));