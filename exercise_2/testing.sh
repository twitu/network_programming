#!/bin/bash

make

declare -a commands=(
		"ls -al | wc" "ps -al"
		"du -ah | grep pipeline"
		"cat /etc/passwd | grep $USER | sort"
		"cat /etc/passwd | grep $USER | cut -d, -f1"
		"ps -al | grep $$"
		)

for command in "${commands[@]}";
do
	echo "running programme with command: $command"
	./pipeline.out "$command"
	sleep 0.3
	echo "==========================================="
done
