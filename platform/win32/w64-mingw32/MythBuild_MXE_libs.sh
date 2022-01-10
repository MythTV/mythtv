#!/bin/bash

echo "Tested on ubuntu 2004. Run this file to build mythtv for Windows"

sudo apt-get --assume-yes install \
     git gcc g++ wget python3 perl bzip2 lzip unzip libssl-dev \
     p7zip make autoconf automake bison flex autopoint gperf \
     libtool libtool-bin ruby intltool p7zip-full python \
     pkg-config yasm mmv

echo "Creating build tree"
export buildPath=$PWD"/build"
mkdir -p $buildPath
cd $buildPath
mkdir -p install/bin/plugins
mkdir -p themes

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
cd ..

echo "Cloning MXE"
git clone https://github.com/mxe/mxe.git

echo "Add SQL to QT"
sed -i 's/-no-sql-mysql /\//g' $buildPath/mxe/src/qt.mk

cd mxe
sudo make cc MXE_PLUGIN_DIRS=plugins/gcc8 MXE_TARGETS='i686-w64-mingw32.shared' vulkan-loader vulkan-headers qt5 nasm yasm libsamplerate taglib zlib gnutls mman-win32 pthreads libxml2 libdvdcss x264 x265 lame libass qtwebkit xvidcore libvpx vorbis flac
cd ..

find . -name \*.dll -exec cp {} \install/bin \;

export PATH=$buildPath"/mxe/usr/bin":$PATH
sudo chmod -R 777 $buildPath/mxe

echo -e "#define RTLD_LAZY 0 \n#define HAVE_DVDCSS_DVDCSS_H" | sudo -E tee $buildPath/mxe/usr/i686-w64-mingw32.shared/include/dlfcn.h

sudo -E sed -i 's/#define GetUserName __MINGW_NAME_AW(GetUserName)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h
sudo -E sed -i 's/#define CopyFile __MINGW_NAME_AW(CopyFile)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h
sudo -E sed -i 's/#define MoveFile __MINGW_NAME_AW(MoveFile)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h

sudo cp $buildPath/mxe/usr/x86_64-pc-linux-gnu/bin/yasm $buildPath//mxe/usr/bin/yasm
sudo cp $buildPath/mxe/usr/bin/i686-w64-mingw32.shared-pkg-config $buildPath//mxe/usr/bin/pkg-config
sudo cp -R $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++ $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include

cp $buildPath/mythtv/platform/win32/w64-mingw32/Patches/libexiv2.patch $buildPath/mythtv/libexiv2.patch
cd mythtv
git apply libexiv2.patch -v
rm libexiv2.patch
cd ..

echo "Compiling libudfread"
GIT_SSL_NO_VERIFY= git clone https://code.videolan.org/videolan/libudfread.git
cd libudfread
./bootstrap
./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --host=i686-w64-mingw32.shared
# libtool won't build this as a shared library without this flag.
sed -i 's/LDFLAGS = /LDFLAGS = -no-undefined/' Makefile
make -j$(nproc)
sudo make install
cd ..

GIT_SSL_NO_VERIFY= git clone https://code.videolan.org/videolan/libbluray.git
cd $buildPath/libbluray
GIT_SSL_NO_VERIFY= git submodule update --init

sudo -E chmod -R 777 $buildPath/libbluray

echo "Compiling libbluray"
./bootstrap
./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --disable-examples --with-freetype --with-libxml2 --disable-bdjava-jar --host=i686-w64-mingw32.shared
make -j$(nproc)
make install
cd ..

echo "Compiling libzip"
git clone https://github.com/nih-at/libzip.git
sudo -E chmod -R 777 $buildPath/libzip
cd libzip
$buildPath/mxe/usr/bin/i686-w64-mingw32.shared-cmake $buildPath/libzip
make -j$(nproc)
make install
cd ..

echo "Compiling SoundTouch"
GIT_SSL_NO_VERIFY= git clone https://codeberg.org/soundtouch/soundtouch.git
sudo -E chmod -R 777 $buildPath/soundtouch
cd soundtouch
./bootstrap
./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --host=i686-w64-mingw32.shared
sed -i 's/LDFLAGS = /LDFLAGS = -no-undefined/' $buildPath/soundtouch/source/SoundTouch/Makefile
make -j$(nproc)
make install
cd ..

echo "Install endian.h"
git clone https://gist.github.com/PkmX/63dd23f28ba885be53a5
sudo -E cp $buildPath/63dd23f28ba885be53a5/portable_endian.h $buildPath/mxe/usr/i686-w64-mingw32.shared/include/endian.h


echo "Done"
