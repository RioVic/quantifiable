#!/bin/bash

Program="./a.out"
Stack="$1"

for i in {1..32}
do
	$Program i 100000 1 $Stack
done

for i in {1..32}
do
	$Program i 10000 10 $Stack
done

for i in {1..32}
do
	$Program i 1000 100 $Stack
done

for i in {1..32}
do
	$Program i 100 1000 $Stack
done

for i in {1..32}
do
	$Program i 10 10000 $Stack
done