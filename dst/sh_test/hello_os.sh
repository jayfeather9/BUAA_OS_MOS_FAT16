#!/bin/bash

cat $1 | tail -n +8 | head -n 1 > $2
cat $1 | tail -n +32 | head -n 1 >> $2
cat $1 | tail -n +128 | head -n 1 >> $2
cat $1 | tail -n +512 | head -n 1 >> $2
cat $1 | tail -n +1024 | head -n 1 >> $2
