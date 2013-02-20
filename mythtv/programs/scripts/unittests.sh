#!/bin/sh
#
# unittests.sh runs all unit tests and returns 0 if all are successful

DIRNAME=`which dirname`
BASENAME=`which basename`
SED=`which sed`
TEST_FAILED=0


TESTS=`find -name "test_*.pro"`

for TEST in $TESTS
do
    PATH=`$DIRNAME $TEST`
    EXEC=`$BASENAME $TEST | $SED -e 's/.pro//'`
    RUNNABLE=$PATH/$EXEC
    if test -x $RUNNABLE -a -f $RUNNABLE ; then
        if ./$RUNNABLE ; then
            echo
        else
            echo "error: A unit test failed."
            TEST_FAILED=1
        fi
    else
        echo "Unable to find test $RUNNABLE, marking as a failed unit test."
        TEST_FAILED=1
    fi
done

if test "x$TEST_FAILED" != "x0" ; then
    echo "error: At least one unit test failed, returning 1"
fi

exit $TEST_FAILED