#!/bin/sh
Program="./a.out"
Threads="$1"

mkdir qstack
mkdir ebs
mkdir treiber

rm -f -r qstack/*
rm -f -r ebs/*
rm -f -r treiber/*

cp -f -r thread$Threads/AMD_EBS*inversions.dat ebs/
cp -f -r thread$Threads/AMD_QStackDesc*inversions.dat qstack/
cp -f -r thread$Threads/AMD_Treiber*inversions.dat treiber/

Rscript paper5-entropyNt_Mi_Kr.R .
