#!/bin/bash

Program="./a.out"

for i in {1..32}
do
        for k in {1..32}
        do
            for j in {1..5}
            do
		echo $Program $i 1000 5000 $k
                $Program $i 1000 5000 $k
            done
        done
done
