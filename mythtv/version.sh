#!/bin/sh
#
# small shell script to generate version.cpp
# it expects two pareameters
# first parameter is the root of the source directory
# second paramater is the svn base folder (trunk, branches/release-0-21-fixes)

if test $# -ne 2; then
    echo "Usage: version.sh SVN_TREE_DIR SVN_REPO_PATH"
    exit 1
fi

SVNTREEDIR=$1
SVNREPOPATH=$(echo $2 | sed -e 's,.*/svn/,,' \
                            -e 's,/mythtv/version\.pro.*,,' \
                            -e 's,/version\.pro.*,,')

SOURCE_VERSION=$(svnversion ${SVNTREEDIR} 2>/dev/null || echo Unknown)

# Get a string like "0.21.20071125-1"
BINARY_VERSION=$(grep MYTH_BINARY_VERSION \
    "${SVNTREEDIR}/libs/libmythdb/mythversion.h" \
    | sed -e 's/.*MYTH_BINARY_VERSION //')

echo "const char *myth_source_version = \"${SOURCE_VERSION}\";" > .vers.new
echo "const char *myth_source_path = \"${SVNREPOPATH}\";" >> .vers.new
echo "const char *myth_binary_version = ${BINARY_VERSION};" >> .vers.new

# check if the version strings are changed and update version.pro if necessary
diff .vers.new version.cpp > .vers.diff 2>&1
if test -s .vers.diff ; then
   mv -f .vers.new version.cpp
fi
rm -f .vers.new .vers.diff
