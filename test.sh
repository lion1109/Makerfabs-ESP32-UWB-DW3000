#!/bin/bash

. ~/esp/esp_idf/export.sh

results=""
for d in example/range/range_{tx,rx} example/simple_test/simple_{tx,rx}; do
	pushd $d
	idf.py set-target esp32
	idf.py fullclean
	idf.py build
	res=$?
	if test "$res" != "0"; then results="$results\nerror in $d"; fi
	idf.py fullclean
	rm -rf build sdkconfig.old sdkconfig
	popd
done

if test "$results" == ""; then
	exit 0
else
	echo -e "\nresults:\n$results"
	exit 1
fi
