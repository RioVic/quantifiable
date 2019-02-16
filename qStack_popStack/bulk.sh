#!/bin/bash

Program="./a.out"
Threads="$1"
Ops="$2"
Ratio="$3"
Stack="$4"


for ((i=1 ; i <= $Threads ; i++ ));
do
	echo $i
	$Program $i $Ops $Ratio $Stack
done