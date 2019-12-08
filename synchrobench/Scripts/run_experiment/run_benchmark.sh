#!/bin/bash

set -o xtrace

ARTIFACT="${ARTIFACT:-lazy-list}"

instrument=../../c-cpp/bin/MUTEX-"$ARTIFACT"-entropy
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

mkdir -p results

for items in 1 2 5 20; do
	item_dir=results/"$items"items
	mkdir -p $item_dir
	for sz in 500 3000; do
		for threads in 2 4 6 15 100 1000; do
			prefix=AMD_"$ARTIFACT"_"$items"i_"$sz"ko_"$threads"t
			let qs=$sz*250
			let hs=$sz*500
			$instrument -a "$qs" -r "$qs" -d "$hs" -t "$threads" -R "$items" -o "$item_dir"/$prefix
			$inverter $item_dir/"$prefix"_parallel.dat $item_dir/"$prefix"_ideal.dat 1
		done
	done
done
