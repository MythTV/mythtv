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
SVNREPOPATH=$(echo "$$URL$$" | sed -e 's,.*/svn/,,' \
                            -e 's,/mythtv/version\.sh.*,,' \
                            -e 's,/version\.sh.*,,')

SOURCE_VERSION=$(svnversion ${SVNTREEDIR} 2>/dev/null || echo Unknown)

case "${SOURCE_VERSION}" in
    exported|Unknown)
        if test -e $SVNTREEDIR/VERSION ; then
            . $SVNTREEDIR/VERSION
        fi
     ;;
esac

echo "#include \"mythexp.h\"" > .vers.new
echo "#include \"mythversion.h\"" >> .vers.new
echo "const MPUBLIC char *myth_source_version = \"${SOURCE_VERSION}\";" >> .vers.new
echo "const MPUBLIC char *myth_source_path = \"${SVNREPOPATH}\";" >> .vers.new
echo "const MPUBLIC char *myth_binary_version = MYTH_BINARY_VERSION;" >> .vers.new

# check if the version strings are changed and update version.pro if necessary
diff .vers.new version.cpp > .vers.diff 2>&1
if test -s .vers.diff ; then
   mv -f .vers.new version.cpp
fi
rm -f .vers.new .vers.diff
