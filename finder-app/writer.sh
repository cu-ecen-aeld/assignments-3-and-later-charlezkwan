#!/bin/sh

writefile=$1
writestr=$2

if [ $# -ne 2 ];
then
    echo "ERROR: Invalid Number of Arguments."
    echo "Total number of arguments should be 2."
    exit 1
fi

if [ ! -d ${writefile%/*} ];
then
    mkdir -p "${writefile%/*}"
fi

if ! echo $writestr > $writefile;
then
    echo "ERROR: file cannot be created"
    exit 1

fi