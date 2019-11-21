#!/bin/bash

set -o xtrace

for items in 1 2 5 20 100 500; do
	item_dir=results/"$items"items
	mkdir $item_dir
	for sz in 1 16 100; do
		Rscript graph.R $item_dir/"$sz"kops_hist.pdf $item_dir/AMD_lazylist_"$items"i_"$sz"ko_*_inversions.dat 
	done

	for threads in 1 2 6; do
		Rscript graph.R $item_dir/"$threads"threads_hist.pdf $item_dir/AMD_lazylist_"$items"i_*_"$threads"t_inversions.dat 
	done
done
