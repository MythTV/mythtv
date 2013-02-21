#!/bin/sh
#
# unittests.sh runs all unit tests and returns 0 if all are successful

DIRNAME=`which dirname`
BASENAME=`which basename`
SED=`which sed`
GCOV=`which gcov`
GREP=`which grep`
TEST_FAILED=0


TESTS=`find -name "test_*.pro"`

for TEST in $TESTS
do
    FPATH=`$DIRNAME $TEST`
    EXEC=`$BASENAME $TEST | $SED -e 's/.pro//'`
    COV=`$BASENAME $TEST | $SED -e 's/test_//' | $SED -e 's/.pro/.cpp/'`
    COVGCNO=`$BASENAME $TEST | $SED -e 's/test_//' | $SED -e 's/.pro/.gcno/'`
    RUNNABLE=$FPATH/$EXEC
    if test -x $RUNNABLE -a -f $RUNNABLE ; then
        if ./$RUNNABLE ; then
            if test -x $GCOV -a -f $FPATH/$COVGCNO ; then
                P=`pwd` ; cd $FPATH # pushd==
                LINES=`$GCOV $COV | $GREP Lines | $SED -e 's/Lines//'`
                echo Coverage: $COV $LINES. See $FPATH/$COV.gcov for details
                cd $P # popd==
            fi
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