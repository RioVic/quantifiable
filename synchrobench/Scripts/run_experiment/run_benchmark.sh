#!/bin/bash

set -o xtrace


instrument=../../c-cpp/bin/MUTEX-lazy-list-entropy
inverter=../a.out

if [ $# -eq 2 ]; then
	instrument=$1
	inverter=$2
fi

pushd ../../c-cpp/src/entropy
make
popd

pushd ../
make
popd

rm -rf results
mkdir results

for items in 1 2 5 20 100 500; do
	item_dir=results/"$items"items
	mkdir $item_dir
	for sz in 1 16 100; do
		for threads in 1 2 6; do
			prefix=AMD_lazylist_"$items"i_"$sz"ko_"$threads"t
			let qs=$sz*250
			let hs=$sz*500
			$instrument -a "$qs" -r "$qs" -d "$hs" -t "$threads" -R "$items" -o "$item_dir"/$prefix
			$inverter $item_dir/"$prefix"_parallel.dat $item_dir/"$prefix"_ideal.dat 1
		done
	done
done
