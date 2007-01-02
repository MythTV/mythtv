#!/bin/sh
# ===========================================================================
# = NAME
# makebundle
#
# = USAGE
# makebundle mythfrontend[.app]
# makebundle mythtv-setup[.app]
# makebundle mythtv[.app]
#
# = DESCRIPTION
# Script to produce an application bundle with filters, themes, et c.
# osx-bundler.pl does most of the hard work, automatically, but the
# other parts are Myth specific, and use lots of nasty hard-coded paths.
#
# = REVISION
# $Id$
#
# = AUTHOR
# Nigel Pearson
# ===========================================================================

if [ -z $1 ] ; then
    echo "Error: No binary/bundle name supplied."
    exit
fi

if [ -e $1 ] ; then
    # Lazy way of guessing if $1 ends in .app:
    if [ -e $1/Contents ] ; then
        BNDL=$1/Contents
    else
        BNDL=$1.app/Contents
    fi
else
    echo "Error: binary/bundle '"$1"' doesn't exist!"
    exit
fi

# ===========================================================================
# Search the binary's libs and frameworks, copy each one into the bundle,
# and then correct the load paths in binary/libs/frameworks. If called
# with a binary, this will also produce the .app directory structure too.

echo "Installing libraries into bundle as Frameworks"
../../contrib/OSX/osx-bundler.pl $1 ../../libs/* $QTDIR/lib
if [ $? -ne 0 ] ; then
    echo
    echo "    ERROR.    osx-bundler.pl failed"
    exit -1
fi

# ===========================================================================
# Install the config/filter/theme files in our bundle:

mkdir -p $BNDL/Resources/lib
mkdir -p $BNDL/Resources/share/mythtv

echo "Installing XML files"
cp *.xml $BNDL/Resources/share/mythtv

# Fake directory to install into:
mkdir -p ./usr/local
ln -sf `pwd`/$BNDL/Resources/* ./usr/local

echo "Installing language translations"
make -C ../../i18n		INSTALL_ROOT=`pwd`	install >/dev/null
echo "Installing standard (basic) themes"
make -C ../../themes		INSTALL_ROOT=`pwd`	install >/dev/null
echo "Installing filters"
make -C ../../filters		INSTALL_ROOT=`pwd`	install >/dev/null
#echo "Installing extra themes"
#make -C ../../../myththemes	INSTALL_ROOT=`pwd`	install >/dev/null

rm -fr ./usr

# ===========================================================================
# Copy in any extra Frameworks that osx-bundler doesn't find:

#cp -pr /Developer/FireWireSDK21/Examples/Framework/AVCVideoServices.framework \
#       $BNDL/Frameworks

#cp -pr /System/Library/Frameworks/DiskArbitration.framework \
#	$BNDL/Frameworks

#/usr/bin/install_name_tool -change /System/Library/Frameworks/DiskArbitration.framework/Versions/A/DiskArbitration \@executable_path/../Frameworks/DiskArbitration.framework/DiskArbitration $BNDL/MacOS/mythfrontend

# ===========================================================================
