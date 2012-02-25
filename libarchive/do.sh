#!/bin/sh
for FILE in *.5; do
LOG=`git log -1 $FILE | grep Date`
echo $FILE: $LOG
done
