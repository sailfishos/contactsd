#! /bin/sh

tmpdir=$(mktemp -d)

export XDG_DATA_HOME=$tmpdir
export XDG_CACHE_HOME=$tmpdir
export XDG_CONFIG_HOME=$tmpdir
export XDG_CACHE_HOME=$tmpdir

tracker-control -rs #2>&1 >/dev/null
/usr/lib/tracker/tracker-store&
/usr/bin/contactsd&

sleep 5

$1
result=$?

tracker-control -r #2>&1 >/dev/null

if [ $result -eq "0" ]; then
    echo "Test succeeded:" $1
else
    echo "Test failed:" $1
fi

rm -rf $tmpdir

exit $result
