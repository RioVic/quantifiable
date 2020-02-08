#!/bin/bash

Program="./a.out"

for i in {1..32}
do
	for k in {1..32}
	do
		echo $Program $i 1000 100 $k 
		$Program $i 1000 100 $k
	done
done

