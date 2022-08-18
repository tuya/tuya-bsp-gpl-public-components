#!/bin/bash

echo $#

if [ $# -ne 1 ]  ; then
	echo "Usage : ./auto_release.sh <Version>"
	exit
fi

make clean > /dev/null
cd ../

package_name="uboot_"$1".tar.gz"

if [ -e $package_name ] ; then
	rm -f $package_name
fi

tar cvfz $package_name --exclude=uboot/auto_release.sh --exclude=uboot/.git uboot/*

`curl "ftp://172.16.6.251/base/uboot/$package_name" -u "release:123456" -T "$package_name"`

rm -f $package_name
