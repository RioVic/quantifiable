#!/bin/bash

Program="$1"
Threads="$2"

for ((i=1 ; i <= $Threads ; i++ ));
do
	$Program $i
done