#!/bin/bash

./bug-lockuse-det 1> /tmp/out;
cat /tmp/out | head -1 > /tmp/lockuse;

for i in `seq 1 20`
do
    ./bug-lockuse-det > /tmp/out;
    cat /tmp/out | head -1 > /tmp/lockuse_tmp;
    det=`diff /tmp/lockuse /tmp/lockuse_tmp | wc -l`;
    if [[ $det = 0 ]]
    then
	echo "fine";
    else
	echo "wrong";
	break;
    fi
done;