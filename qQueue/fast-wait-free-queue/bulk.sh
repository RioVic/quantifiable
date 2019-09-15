#!/bin/bash

for i in {1..32}
do
	./msqueue $i 100000 1 0
	./msqueue $i 100000 1 1
	./faa $i 100000 1 0
	./faa $i 100000 1 1
	./ccqueue $i 100000 1 0
	./ccqueue $i 100000 1 1
done

for i in {1..32}
do
	./msqueue $i 10000 10 0
	./msqueue $i 10000 10 1
	./faa $i 10000 10 0
	./faa $i 10000 10 1
	./ccqueue $i 10000 10 0
	./ccqueue $i 10000 10 1
done

for i in {1..32}
do
	./msqueue $i 1000 100 0
	./msqueue $i 1000 100 1
	./faa $i 1000 100 0
	./faa $i 1000 100 1
	./ccqueue $i 1000 100 0
	./ccqueue $i 1000 100 1
done

for i in {1..32}
do
	./msqueue $i 100 1000 0
	./msqueue $i 100 1000 1
	./faa $i 100 1000 0
	./faa $i 100 1000 1
	./ccqueue $i 100 1000 0
	./ccqueue $i 100 1000 1
done