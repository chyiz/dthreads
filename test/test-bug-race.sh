#!/bin/bash

for i in `seq 1 100`
do
	./bug-race-det > /tmp/blah;
	det=`cat /tmp/blah | grep "52 55 58 61 64" | wc -l`;
	echo $det;
	if [[ $det = 0 ]]
	then
		echo "wrong";
		break;
	else
		echo "fine";
	fi;
done;
