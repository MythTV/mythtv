#!/bin/bash

echo "Tested on ubuntu 2004. Run this file to build mythtv for Windows"

sudo apt-get --assume-yes install \
     git gcc g++ wget python3 perl bzip2 lzip unzip libssl-dev \
     p7zip make autoconf automake bison flex autopoint gperf \
     libtool libtool-bin ruby intltool p7zip-full \
     pkg-config yasm mmv

buildRoot=$PWD
while test ! -e "$buildRoot/.git" ; do
    buildRoot=${buildRoot%/*}
    if test "x$buildRoot" == "x" ; then
        echo "Cannot find .git file or directory.  Exiting."
        exit
    fi
done
export buildRoot
echo "Build root is $buildRoot"

echo "Settings paths"
export buildPath=$buildRoot"/build"
export PATH=$buildPath"/mxe/usr/bin":$PATH

if test "x$1" == "xclean" ; then
    echo "Removing build tree"
    rm -rf $buildPath
fi

if test -e "$buildPath" ; then
    echo "Build tree already exists"
else
    echo "Creating build tree"
    mkdir -p $buildPath/install/bin/plugins
    mkdir -p $buildPath/themes
fi

if test -e "$buildPath/themes/Mythbuntu-classic" ; then
    echo "MythTV themes already exist"
else
    echo "Cloning MythTV themes"
    cd $buildPath/themes
    git clone https://github.com/paul-h/MythCenterXMAS-wide.git
    git clone https://github.com/wesnewell/Functionality
    git clone https://github.com/MythTV-Themes/TintedGlass
    git clone https://github.com/MythTV-Themes/Readability
    git clone https://github.com/MythTV-Themes/Steppes
    git clone https://github.com/MythTV-Themes/Retro-wide
    git clone https://github.com/MythTV-Themes/LCARS
    git clone https://github.com/MythTV-Themes/Childish
    git clone https://github.com/MythTV-Themes/Arclight
    git clone https://github.com/MythTV-Themes/Mythbuntu
    git clone https://github.com/MythTV-Themes/blue-abstract-wide
    git clone https://github.com/MythTV-Themes/Mythbuntu-classic
fi

cd $buildPath
if test -d "mxe" ; then
    echo "MXE already exists"
else
    echo "Cloning MXE"
    git clone https://github.com/mxe/mxe.git

    echo "Add SQL to QT"
    sed -i 's/-no-sql-mysql /\//g' $buildPath/mxe/src/qt.mk

    cd mxe
    make cc MXE_PLUGIN_DIRS=plugins/gcc8 MXE_TARGETS='i686-w64-mingw32.shared' vulkan-loader vulkan-headers qt5 nasm yasm libsamplerate taglib zlib gnutls mman-win32 pthreads libxml2 libdvdcss x264 x265 lame libass qtwebkit xvidcore libvpx vorbis flac
    if test $? != 0 ; then
	echo "Failed to build mxe."
	exit
    fi
    cd ..

    find . -name \*.dll -exec cp {} \install/bin \;

    chmod -R 755 $buildPath/mxe

    echo -e "#define RTLD_LAZY 0 \n#define HAVE_DVDCSS_DVDCSS_H" | tee $buildPath/mxe/usr/i686-w64-mingw32.shared/include/dlfcn.h

    sed -i -e 's/#define GetUserName __MINGW_NAME_AW(GetUserName)//g' \
	-e 's/#define CopyFile __MINGW_NAME_AW(CopyFile)//g'       \
	-e 's/#define MoveFile __MINGW_NAME_AW(MoveFile)//g'       \
	$buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h

    #sudo apt-get --assume-yes remove yasm

    cp $buildPath/mxe/usr/x86_64-pc-linux-gnu/bin/yasm $buildPath/mxe/usr/bin/yasm
    cp $buildPath/mxe/usr/bin/i686-w64-mingw32.shared-pkg-config $buildPath/mxe/usr/bin/pkg-config
    cp -R $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++ $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include
fi


cd $buildPath
if test -d "libudfread" ; then
    echo "Directory libudfread already exists"
else
    echo "Compiling libudfread"
    git clone https://code.videolan.org/videolan/libudfread.git
    cd libudfread
    ./bootstrap
    ./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --host=i686-w64-mingw32.shared
    # libtool won't build this as a shared library without this flag.
    sed -i 's/LDFLAGS = /LDFLAGS = -no-undefined/' Makefile
    make -j$(nproc)
    if test $? != 0 ; then
	echo "Failed to build libudfread."
	exit
    fi
    make install
fi


cd $buildPath
if test -d "libbluray" ; then
    echo "Directory libbluray already exists"
else
    git clone https://code.videolan.org/videolan/libbluray.git
    chmod -R 755 libbluray
    cd libbluray
    git submodule update --init

    echo "Compiling libbluray"
    ./bootstrap
    ./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --disable-examples --with-freetype --with-libxml2 --disable-bdjava-jar --host=i686-w64-mingw32.shared
    make -j$(nproc)
    if test $? != 0 ; then
	echo "Failed to build libbluray."
	exit
    fi
    make install
fi


cd $buildPath
if test -d "libzip" ; then
    echo "Directory libzip already exists"
else
    echo "Compiling libzip"
    git clone https://github.com/nih-at/libzip.git
    chmod -R 755 libzip
    cd libzip
    $buildPath/mxe/usr/bin/i686-w64-mingw32.shared-cmake $buildPath/libzip
    make -j$(nproc)
    if test $? != 0 ; then
	echo "Failed to build libzip."
	exit
    fi
    make install
fi


cd $buildPath
if test -d "soundtouch" ; then
    echo "Directory soundtouch already exists"
else
    echo "Compiling SoundTouch"
    git clone https://codeberg.org/soundtouch/soundtouch.git
    chmod -R 755 soundtouch
    cd soundtouch
    ./bootstrap
    ./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --host=i686-w64-mingw32.shared
    sed -i 's/LDFLAGS = /LDFLAGS = -no-undefined/' $buildPath/soundtouch/source/SoundTouch/Makefile
    make -j$(nproc)
    if test $? != 0 ; then
	echo "Failed to build soundtouch."
	exit
    fi
    make install
fi


cd $buildPath
if test -f "$buildPath/mxe/usr/i686-w64-mingw32.shared/include/endian.h" ; then
    echo "Endian.h already exists"
else
    echo "Install endian.h"
    git clone https://gist.github.com/PkmX/63dd23f28ba885be53a5 portable_endian
    cp $buildPath/portable_endian/portable_endian.h $buildPath/mxe/usr/i686-w64-mingw32.shared/include/endian.h
fi


echo "Done"
