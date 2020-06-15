#!/bin/sh
#
# unittests.sh runs all unit tests and returns 0 if all are successful

if test "x$BASH_VERSION" = "x"; then
    export FF_CONF_EXEC
    if test "0$FF_CONF_EXEC" -lt 1; then
        FF_CONF_EXEC=1
        exec bash "$0" "$@"
    fi
    echo "This script needs bash to run."
    exit 1
fi

DIRNAME=`which dirname`
BASENAME=`which basename`
SED=`which sed`
GCOV=`which gcov`
GREP=`which grep`
TEST_FAILED=0

if test "x$(uname -s)" = "xFreeBSD"; then
    # Find all shared libraries (".so" on linux/freebsd, ".dylib" on OSX).
    # Eliminate multiples on OSX because of where the suffix is added.
    declare -a DIRS
    LIBS=$(find $(dirname $(pwd)) -name "*.so" -o -name "*.dylib")
    for lib in $LIBS ; do
        DIR=$(dirname $lib)
        if [[ ! ${DIRS[*]} =~ ${DIR} ]]; then
        DIRS+=($DIR)
        fi
    done
    export LD_LIBRARY_PATH=$(echo ${DIRS[*]} | tr ' ' ':')
fi

TESTS=`find . -name "test_*.pro"`

for TEST in $TESTS
do
    FPATH=`$DIRNAME $TEST`
    EXEC=`$BASENAME $TEST | $SED -e 's/\.pro//'`
    COV=`$BASENAME $TEST | $SED -e 's/test_//' | $SED -e 's/\.pro/.cpp/'`
    COVGCNO=`$BASENAME $TEST | $SED -e 's/test_//' | $SED -e 's/\.pro/.gcno/'`
    RUNNABLE=$FPATH/$EXEC
    if test -x $RUNNABLE -a -f $RUNNABLE ; then
        if ./$RUNNABLE ; then
            if test -x "$GCOV" -a -f $FPATH/$COVGCNO ; then
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
