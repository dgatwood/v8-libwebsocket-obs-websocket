#!/bin/sh

for PATHTOCHECK in "/usr/local/include" "$(deps/get_ssl_dir.sh)/include" ; do
    if [ -d "$PATHTOCHECK/openssl" ] ; then
	echo "$PATHTOCHECK"
        exit 0
    fi
done
