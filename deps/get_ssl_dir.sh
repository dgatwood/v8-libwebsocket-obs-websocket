#!/bin/sh

for PATHTOCHECK in /usr/local/opt/openssl /usr/local/ssl ; do
    if [ -d "$PATHTOCHECK" ] ; then
	echo "$PATHTOCHECK"
        exit 0
    fi
done
