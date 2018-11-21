#!/bin/bash

make

child_process=(10 20 40 80 160 320, 12, 53, 89)
for number in ${child_process[*]}
do
	echo "running programme for $number child processes"
	./signal.out $number
	echo "==========================================="
done
