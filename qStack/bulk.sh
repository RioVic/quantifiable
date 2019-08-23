#!/bin/bash

Program="./a.out"
Stack="$1"

$Program 4 1000000 1 $Stack
$Program 8 1000000 1 $Stack
$Program 16 1000000 1 $Stack
$Program 32 1000000 1 $Stack

$Program 4 100000 10 $Stack
$Program 8 100000 10 $Stack
$Program 16 100000 10 $Stack
$Program 32 100000 10 $Stack

$Program 4 10000 100 $Stack
$Program 8 10000 100 $Stack
$Program 16 10000 100 $Stack
$Program 32 10000 100 $Stack