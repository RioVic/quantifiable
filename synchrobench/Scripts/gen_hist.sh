#!/bin/sh
Program="./a.out"
Threads="$1"

Rscript paper5-entropyNt_Mi_Kr.R .
Rscript paper5-entropyNt_Mi_Kr_2.R .
mv hist.pdf hist_$1t.pdf
mv raw.pdf raw_$1t.pdf 
