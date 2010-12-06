#!/bin/sh
COVERAGE_TEST_DIRS="
    ut_contactsd
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
