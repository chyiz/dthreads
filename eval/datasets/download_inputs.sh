#!/bin/bash

apps="blackscholes ferret"
kernels="dedup canneal"
sizes="small medium large"

printf "\n\ndownloading parsec...\n\n";
#wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-input-sim.tar.gz;
printf "\n\nextracting parsec...\n\n";

#tar xvzf parsec-3.0-input-sim.tar.gz;

for prog in $apps;
do 
    echo $prog;
    rm -rf $prog &> /dev/null;
    mkdir $prog;
    for size in $sizes 
    do
        tar xvf parsec-3.0/pkgs/apps/$prog/inputs/input_sim$size.tar -C $prog;
    done;
done;

for prog in $kernels;
do
    echo $prog;
    rm -rf $prog &> /dev/null;
    mkdir $prog;
    for size in $sizes
    do
        tar xvf parsec-3.0/pkgs/kernels/$prog/inputs/input_sim$size.tar -C $prog;
	if [ $prog = "dedup" ]
	then
	    mv dedup/media.dat dedup/media_$size.dat;
	fi
    done;
done;