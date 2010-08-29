#!/bin/sh
#
# small shell script to generate version.cpp
# it expects two parameters
# first parameter is the root of the source directory
# second parameter is the svn base folder (trunk, branches/release-0-21-fixes)

if test $# -ne 1; then
    echo "Usage: version.sh SVN_TREE_DIR"
    exit 1
fi

TESTFN=`mktemp $1/.test-write-XXXXXX` 2> /dev/null
if test x$TESTFN != x"" ; then
    rm -f $TESTFN
else
    echo "$0: Can not write to destination, skipping.."
    exit 0
fi

SVNTREEDIR=$1
SVNREPOPATH="exported"

SOURCE_VERSION=$(svnversion ${SVNTREEDIR} 2>/dev/null || echo Unknown)

case "${SOURCE_VERSION}" in
    exported|Unknown)
        if test -e $SVNTREEDIR/VERSION ; then
            . $SVNTREEDIR/VERSION
        fi
    ;;
    *)
    SVNREPOPATH=$(echo "$$URL$$" | sed -e 's,.*/svn/,,' \
                                       -e 's,/mythtv/version\.sh.*,,' \
                                       -e 's,/version\.sh.*,,')
    ;;
esac

cat > .vers.new <<EOF
#include "mythexp.h"
#include "mythversion.h"

const MPUBLIC char *myth_source_version = "${SOURCE_VERSION}";
const MPUBLIC char *myth_source_path = "${SVNREPOPATH}";
const MPUBLIC char *myth_binary_version = MYTH_BINARY_VERSION;
EOF

# check if the version strings are changed and update version.pro if necessary
if ! cmp -s .vers.new version.cpp; then
   mv -f .vers.new version.cpp
fi
rm -f .vers.new
