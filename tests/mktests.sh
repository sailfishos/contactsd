#!/bin/bash -e

package=qtcontacts-tracker-tests

srcdir=`dirname "$0"`
srcdir=`cd "$srcdir" && pwd`

cat <<EOF
<testdefinition version="0.1">
  <suite name="$package">
    <description>Unit and regression tests for QtContacts' tracker backend</description>
    <set name="$package">
EOF

for suite in "$@"
do
    test -f "$suite/$suite.skip" && continue

    cat <<EOF
EOF

    "$suite/$suite" -functions | sed -ne 's/()$//p' | while read test
    do
        attributes="name=\"$suite-$test\""
        description=`grep "^$suite::$test\\>" $srcdir/../EXPECTFAIL || true`

        if test -n "$description"
        then
            attributes="$attributes insignificant=\"true\""
        else
            description="$suite::$test(): no description available"
        fi

        cat <<EOF
      <case $attributes>
        <description>$description</description>
        <step>/usr/bin/$suite $test</step>
      </case>
EOF
    done

    cat <<EOF
      <environments>
        <scratchbox>true</scratchbox>
        <hardware>true</hardware>
      </environments>
EOF

done

cat <<EOF
    </set>
  </suite>
</testdefinition>
EOF
