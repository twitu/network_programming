#!/bin/bash

make

declare -a number=(
		2
        5
        27
        300
		)

for n in "${number[@]}";
do
	echo "running programme with $n philosphers"
	./dining_philosopher $n
	sleep 0.1
    ps -al | grep $$ | cut -d\  -f 5 | kill
	echo "==========================================="
done