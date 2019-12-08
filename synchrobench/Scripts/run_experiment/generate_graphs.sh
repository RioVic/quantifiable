#!/bin/bash

set -o xtrace

ARTIFACT=lazy-list

for items in 1 2 5 20; do
	item_dir=results/"$items"items
	mkdir $item_dir
	for sz in 500 3000; do
		Rscript graph.R $item_dir/"$ARTIFACT"_"$sz"kops_hist.pdf $item_dir/AMD_"$ARTIFACT"_"$items"i_"$sz"ko_*_inversions.dat 
	done

	for threads in 2 4 6 15 100 1000; do
		Rscript graph.R $item_dir/"$ARTIFACT"_"$threads"threads_hist.pdf $item_dir/AMD_"$ARTIFACT"_"$items"i_*_"$threads"t_inversions.dat 
	done
done
