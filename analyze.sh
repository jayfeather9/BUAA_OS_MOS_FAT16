#!/bin/bash

if [ $# -eq 1 ];
then
    # Your code here. (1/4)
    egrep "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} [WE].*" $1 > bug.txt

else
    case $2 in
    "--latest")
        # Your code here. (2/4)
        cat $1 | tail -n 5
    ;;
    "--find")
        # Your code here. (3/4)
        egrep $3 $1 > $3.txt
    ;;
    "--diff")
        # Your code here. (4/4)
        if diff $1 $3 > /dev/null; then echo same; else echo different; fi
    ;;
    esac
fi
