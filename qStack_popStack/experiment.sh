#!/bin/bash

Threads="$1"
rm *.dat

for ((i=1 ; i <= $Threads ; i+=2 ));
do
	echo $i
	for ((k=1 ; k <= 5 ; k ++ ))
	do
        ./a.out $i 1000000 75 QStack
        ./a.out $i 1000000 75 QStack_No_Branch
        #./a.out $i 1000000 75 QStack_Depth_Push
        ./a.out $i 1000000 75 Treiber
        ./a.out $i 1000000 75 EBS
	done
done

