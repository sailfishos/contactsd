#!/bin/bash -e

testdir=`dirname "$0"`
testdir=`cd "$testdir" && pwd`

run() {
    fixture=$1; shift
    cd "$testdir/$fixture"

    tracker-control -rs

    echo "********* Engine parameters:${1:+ $1} *********"
    eval "QT_CONTACTS_TRACKER='$1' ./$fixture" || true
}

run ut_contactsd
# run ut_trackersink 'query-builder=fetch,save'
