#!/usr/bin/bash

echo -n > $2
for newId in $(cat $1); do
    result=$( ./device-id-norm-gen.sh ${newId} )
    if (( $? != 0 )); then
        echo "ERROR: ./device-id-norm-gen.sh ${newId}"
    else
        echo "${result}" >> $2
    fi
done;
exit 0
