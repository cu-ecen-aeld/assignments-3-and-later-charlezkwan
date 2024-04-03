#!/bin/sh

filesdir=$1
searchstr=$2

if [ $# -ne 2 ];
then
    echo "ERROR: Invalid Number of Arguments."
    echo "Total number of arguments should be 2."
    exit 1
fi

if [ ! -d $filesdir ];
then
    echo "ERROR: First arguemnt is invalid."
    echo "First argument should be a directory."
    exit 1
fi



x=$(eval "ls $1 -1 | wc -l")
pwd=$(eval "pwd")
y=$(eval "grep "$2" -r $1 | wc -l")
echo "The number of files are $x and the number of matching lines are $y"


   
