#/bin/bash

#the dthreads guys have a script written in python to do this, but they do alot of turning on/off processors
#and use makefiles for running these programs. I'm more of a bash guy :) plus I want to remove that on/off procs stuff
#and just change threads instead

function usage(){
	printf "\n\n*********usage****************\n\n";
	echo "-p : the number of processors to use. this informs how we spawn threads.";
	echo "-b : a comma delimited list of programs we want to run. Options are: " 
	printf "\tdedup,blacksholes,ferret,canneal,streamcluster\n";
	echo "-l : a comma delimited list of libraries to experiment with. The options are:";
	printf "\tpthread,dthread,dthread_cv\n";
	echo "-t : number of tests to do per program";
	printf "\n\n\n"
}

benchmarks="dedup ferret canneal"
procs=8
libs="pthread dthread"
tests=3

while getopts ":p:b:hl:t:" Option
# Initial declaration.
# c, s, and d are the flags expected.
# The : after flag 'c' shows it will have an option passed with it.
do
case $Option in
# w ) CMD=$OPTARG; FILENAME="PIMSLogList.txt"; TARGET="logfiles"; ;;
p ) procs=$OPTARG ;;
b ) benchmarks=`echo $OPTARG | sed s/','/' '/g`; ;;
h ) usage; exit 1;;
l ) libs=$OPTARG ;;
t ) tests=$OPTARG ;;
* ) echo "Not recognized argument"; exit -1 ;;
esac
done
shift $(($OPTIND - 1))

#ok, we've got the stuff we are running, lets get going
for lib in $libs;
do
for prog in $benchmarks;
do
	#move to the directory
	pushd tests/$prog;
		args=`cat Makefile | grep TEST_ARGS | sed s/"TEST_ARGS = "/""/g | sed s/'\$(DATASET\_HOME)'/'..\/..\/datasets'/g`;
		args=`echo $args | sed s/'\$(THREADS)'/$procs/g`
		echo $args;
		rm /tmp/$prog"_"$lib &> /dev/null;
		for test in `seq 1 $tests`
		do
			(time ./$prog"-"$lib $args) #1> /dev/null 2> /tmp/eval_results;
			#cat /tmp/eval_results | grep "real" | awk -F 'm' '{print $2}' >> /tmp/$prog"_"$lib
		done
		cat /tmp/$prog"_"$lib;
	#leave that directory
	popd;
done;
done;

