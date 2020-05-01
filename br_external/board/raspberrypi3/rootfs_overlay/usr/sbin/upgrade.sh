#!/bin/sh

. /etc/version
#echo $version

host="192.168.1.12:3005"

distant_version=`wget http://$host/latest -q -O -  | jq '.latest'`
url=`wget http://$host/latest -q -O -  | jq '.url'`
url=`echo "$url" | tr -d '"'`
distant_sha=`wget http://$host/latest -q -O -  | jq '.sha256'`
distant_sha=`echo "$distant_sha" | tr -d '"'`
#echo $distant_version
#echo $url
#echo $distant_sha
if [ $distant_version -gt $version ]; then
    echo "donloading http://$host/$url"
    wget http://$host/$url -q -O /tmp/fw.ota
    sha=`sha256sum /tmp/fw.ota | cut -d ' ' -f1`
    if [ "$sha" = "$distant_sha" ]; then
        echo "SHA Matched"
        umount /mnt
        mount /dev/mmcblk0p1 /mnt/
        cp /tmp/fw.ota /mnt/zImage
        sync
        umount /mnt
    else
        echo "SHA doesn't matched $sha $distant_sha"
    fi;
else
    echo "Nothing to upgrade"
fi
