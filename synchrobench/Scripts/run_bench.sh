#!/bin/bash

Program="$1"
THREADS="$2"


for ((i=1;i<=THREADS;i++));
do
	echo "$i"
	for k in {1..10}
	do
		$Program "-a 10000 -d 10000 -r 80000 -R 256 -t $i -o 256k_80r_"$i"t_"$k""
	done
done

for ((i=1;i<=THREADS;i++));
do
	echo "$i"
	for k in {1..10}
	do
		$Program "-a 20000 -d 20000 -r 60000 -R 256 -t $i -o 256k_60r_"$i"t_"$k""
	done
done

for ((i=1;i<=THREADS;i++));
do
	echo "$i"
	for k in {1..10}
	do
		$Program "-a 30000 -d 30000 -r 40000 -R 256 -t $i -o 256k_40r_"$i"t_"$k""
	done
done

for ((i=1;i<=THREADS;i++));
do
	echo "$i"
	for k in {1..10}
	do
		$Program "-a 40000 -d 40000 -r 20000 -R 256 -t $i -o 256k_20r_"$i"t_"$k""
	done
done

for ((i=1;i<=THREADS;i++));
do
	echo "$i"
	for k in {1..10}
	do
		$Program "-a 50000 -d 50000 -r 0 -R 256 -t $i -o 256k_00r_"$i"t_"$k""
	done
done