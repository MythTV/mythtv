#!/bin/sh
#
# small shell script to generate version.h
# it expects one parameter
# first parameter is the root of the source directory
if test $# -ne 1; then
    echo "Usage: version.sh GIT_TREE_DIR"
    exit 1
fi

TESTFN=`mktemp $1/.test-write-XXXXXX` 2> /dev/null
if test x$TESTFN != x"" ; then
    rm -f $TESTFN
else
    echo "$0: Can not write to destination, skipping.."
    exit 0
fi

GITTREEDIR=$1
GITREPOPATH="exported"

cd ${GITTREEDIR}

# if we have a mythtv/DESCRIBE file use that to get the branch and version
if test -e $GITTREEDIR/DESCRIBE ; then
    echo "Using $GITTREEDIR/DESCRIBE"
    . $GITTREEDIR/DESCRIBE
    echo "BRANCH: $BRANCH"
    echo "SOURCE_VERSION: $SOURCE_VERSION"
else
    # get the branch and version from git or fall back to EXPORTED_VERSION then SRC_VERSION as last resort
    git status > /dev/null 2>&1
    SOURCE_VERSION=$(git describe --dirty || git describe || echo Unknown)
    echo "SOURCE_VERSION: $SOURCE_VERSION"

    case "${SOURCE_VERSION}" in
        exported|Unknown)
            if ! grep -q Format $GITTREEDIR/EXPORTED_VERSION; then
                . $GITTREEDIR/EXPORTED_VERSION
                echo "Using $GITTREEDIR/EXPORTED_VERSION"
                echo "BRANCH: $BRANCH"
                echo "SOURCE_VERSION: $SOURCE_VERSION"
                # This file has SOURCE_VERSION and BRANCH
                # example SOURCE_VERSION="30d8a96"
                # BRANCH examples from github
                # BRANCH=" (HEAD -> master)"
                # BRANCH=" (fixes/0.28)"
                # BRANCH=" (tag: v0.28.1)"
                # From a checkout they can be as follows:
                # " (origin/fixes/0.28, fixes/0.28)"
                # " (HEAD -> master, origin/master, origin/HEAD)"
                # " (tag: v0.28.1)"
                hash="$SOURCE_VERSION"
                # This extracts after the last comma inside the parens:
                BRANCH=$(echo "${BRANCH}" | sed -e 's/ (\(.*, \)\{0,1\}\(.*\))/\2/' -e 's,origin/,,')
                # Create a suitable version (hash is no good)
                SOURCE_VERSION="$BRANCH"
                SOURCE_VERSION=`echo "$SOURCE_VERSION" | sed "s/tag: *//"`
                if ! echo "$SOURCE_VERSION" | grep "^v[0-9]" ; then
                    . $GITTREEDIR/SRC_VERSION
                fi
                SOURCE_VERSION="${SOURCE_VERSION}-${hash}"
                echo "Source Version created as $SOURCE_VERSION"
                echo "Branch created as $BRANCH"
            elif test -e $GITTREEDIR/SRC_VERSION ; then
                echo "Using $GITTREEDIR/SRC_VERSION"
                . $GITTREEDIR/SRC_VERSION
                echo "BRANCH: $BRANCH"
                echo "SOURCE_VERSION: $SOURCE_VERSION"
            fi
        ;;
        *)
            if [ -z "${BRANCH}" ]; then
                BRANCH=$(git branch --no-color | sed -e '/^[^\*]/d' -e 's/^\* //' -e 's/(no branch)/exported/')
                echo "Using git to get branch and version"
                echo "BRANCH: $BRANCH"
                echo "SOURCE_VERSION: $SOURCE_VERSION"
            fi
        ;;
    esac
fi

if ! echo "${SOURCE_VERSION}" | grep -Ei "v[0-9]+.*"   ; then
    # Invalid version - use SRC_VERSION file
    echo "WARNING: Invalid source version ${SOURCE_VERSION}, must start with v and a number, will use SRC_VERSION file instead"
    . $GITTREEDIR/SRC_VERSION
fi

src_vn=`echo "${SOURCE_VERSION}" | sed "s/^[Vv]// ; s/-.*// ; s/\..*//"`
MYTH_BINARY_VERSION=`grep -m 1 "MYTHTV_BINARY_VERSION" configure \
  | sed 's/.*"\(.*\)".*/\1/'`

bin_vn=`echo $MYTH_BINARY_VERSION | sed 's/\..*//'`

if ! test $src_vn -eq $bin_vn ; then
    echo "ERROR: High level of source version ${SOURCE_VERSION}, does not match high level of binary version $MYTH_BINARY_VERSION"
    exit 2
fi

cat > .vers.new <<EOF
#ifndef MYTH_SOURCE_VERSION_H
#define MYTH_SOURCE_VERSION_H
static constexpr const char* MYTH_SOURCE_VERSION { "${SOURCE_VERSION}" };
static constexpr const char* MYTH_SOURCE_PATH    { "${BRANCH}" };
#endif
EOF

# check if the version strings are changed and update version.pro if necessary
if ! cmp -s .vers.new libs/libmythbase/version.h; then
   mv -f .vers.new libs/libmythbase/version.h
fi
rm -f .vers.new
