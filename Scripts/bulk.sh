#!/bin/bash

Directory="$1"
Files="${Directory}/*"
Program="./a.out"

for f in $Files
do
	name=${f/#"${Directory}/"}
	case $name in *_ideal.dat) 
		name_no_suffix=${name/%"_ideal.dat"}
		for f in $Files
		do
			name2=${f/#"${Directory}/"}

			case $name2 in "${name_no_suffix}_parallel.dat")
				echo "Running $Program $name2 $name"
				$Program $name2 $name
				;;
			esac
		done
		;; 
	esac
done