#! /bin/bash

if [ $# != 2 ]; then
    echo "Usage: $0 <file> <device>"
    exit -1
fi

if [ `whoami` != 'root' ]; then
    echo 'You must be root'
    exit -1
fi

file=$1
dev=$2

losetup "$dev" "$file"
