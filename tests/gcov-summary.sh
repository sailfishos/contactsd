#!/bin/sh

# This file is part of Contacts daemon
#
# Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
#
# Contact:  Nokia Corporation (info@qt.nokia.com)
#
# GNU Lesser General Public License Usage
# This file may be used under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation and appearing in the
# file LICENSE.LGPL included in the packaging of this file.  Please review the
# following information to ensure the GNU Lesser General Public License version
# 2.1 requirements will be met:
# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# In addition, as a special exception, Nokia gives you certain additional rights.
# These rights are described in the Nokia Qt LGPL Exception version 1.1, included
# in the file LGPL_EXCEPTION.txt in this package.
#
# Other Usage
# Alternatively, this file may be used in accordance with the terms and
# conditions contained in a signed written agreement between you and Nokia.

COVERAGE_TEST_DIRS="
    ut_telepathyplugin
    "

rm gcov.analysis.summary &> /dev/null

function gcov_summary() {
  for dir in $COVERAGE_TEST_DIRS
  do
      find $dir -name "gcov.analysis" -exec cat {} \; | ./gcov.py >> gcov.analysis.summary
  done

  echo -e "\n"
  echo -e "Coverage summary for dirs: "$COVERAGE_TEST_DIRS ": \n"
  cat gcov.analysis.summary
  echo;
}

function lcov_generation() {
    if [ -z $1 ]; then
        LCOV_OUTPUT_DIR="reports/"
    else 
        LCOV_OUTPUT_DIR=$1
    fi

    for dir in $COVERAGE_TEST_DIRS
    do
        lcov  --directory $dir --capture --output-file /tmp/people_tests-$dir.info -b $dir
        lcov -a /tmp/people_tests-$dir.info -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "*targets*" -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "tests/*" -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "*usr/*" -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "*moc*" -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "contactsdplugininterface.h" -o /tmp/people_tests-$dir.addinfo
        lcov -r /tmp/people_tests-$dir.addinfo "contactsimportprogressadaptor.cpp" -o /tmp/people_tests-$dir.addinfo
    done;

    genhtml -o $LCOV_OUTPUT_DIR $(find /tmp -name '*addinfo');
    rm /tmp/people_tests-*info 

    echo -e "\n\nNow point your browser to "$LCOV_OUTPUT_DIR"\n\n"
}

if [ "$1" = "lcov" ]; then
    lcov_generation $2
else 
    gcov_summary
fi
