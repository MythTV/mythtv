#!/bin/bash

buildRoot=$PWD
while test ! -e "$buildRoot/.git" ; do
    buildRoot=${buildRoot%/*}
    if test "x$buildRoot" == "x" ; then
        echo "Cannot find root directory containing .git file or directory.  Exiting."
        exit
    fi
done
export buildRoot
echo "Build root is $buildRoot"

echo "Settings paths"
export buildPath=$buildRoot"/build"
export PATH=$buildPath"/mxe/usr/bin":$PATH
export qt5=$buildPath"/mxe/usr/i686-w64-mingw32.shared/qt5"
export in1=$buildPath"/mxe/usr/i686-w64-mingw32.shared"

if test "x$1" == "xclean" ; then
    echo "Removing MythTV build directory"
    rm -rf $buildPath/mythtv
fi

if test -e "$buildPath/mythtv" ; then
    echo "MythTV build directory exists"
else
    echo "Creating MythTV build directory"
    mkdir -p $buildPath/mythtv
    cd $buildRoot
    cp -al mythplugins mythtv platform themestringstool $buildPath/mythtv
fi

cd $buildPath/mythtv
if test -e .exif_patched ; then
    echo "libexif source already patched"
else
    echo "Patching libexif source"
    patch -p1 < platform/win32/w64-mingw32/Patches/libexiv2.patch
    touch .exif_patched
fi

echo "Compiling mythtv"
cd $buildPath/mythtv/mythtv
./configure --prefix="$buildPath/install" --enable-cross-compile --cross-prefix=i686-w64-mingw32.shared- --target_os=mingw32 --arch=x86 --cpu=pentium3 --qmake=$qt5/bin/qmake --extra-cflags=-I$in1/include-I/home/ubuntu/Desktop/build/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++/i686-w64-mingw32.shared --extra-ldflags=-L$in1/lib --disable-lirc --disable-hdhomerun --disable-firewire --disable-vdpau  --disable-nvdec --disable-dxva2 --enable-libmp3lame --enable-libx264 --enable-libx265 --enable-libxvid --enable-libvpx --disable-w32threads --enable-silent_cc
if test $? != 0 ; then
    echo "Configure failed."
    exit
fi

make -j$(nproc)
if test $? != 0 ; then
    echo "Make failed."
    exit
fi

cp $buildPath/mythtv/mythtv/external/FFmpeg/ffmpeg_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffmpeg.exe
cp $buildPath/mythtv/mythtv/external/FFmpeg/ffprobe_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffprobe.exe
make install
if test $? != 0 ; then
    echo "Make install failed."
    exit
fi

# Hack warning. The libxxx.dll.a files get installed as liblibxxx.dll.a.
cd $buildPath/install/lib
mmv -d 'liblib*' 'lib#1'

cd $buildPath/mythtv/mythplugins
./configure --prefix="$buildPath/install" -cross-prefix=i686-w64-mingw32.shared- --disable-mytharchive
make -j$(nproc) install
if test $? != 0 ; then
    echo "Make plugins failed."
    exit
fi

cp -R $buildPath/mythtv/platform/win32/w64-mingw32/Installer/. $buildPath/install/
#cp -R $buildPath/mythtv/mythtv/src/COPYING $buildPath/install/COPYING
rsync -a --exclude=".git" $buildPath/themes $buildPath/install/share/mythtv/

cd $buildPath"/install/bin"

mv opengl32.dll SOFTWARE_opengl32.dll
rm opengl32.dll

mv libmythgame.dll plugins/libmythgame.dll
mv libmythnews.dll plugins/libmythnews.dll
mv libmythmusic.dll plugins/libmythmusic.dll
mv libmythbrowser.dll plugins/libmythbrowser.dll
mv libmythzoneminder.dll plugins/libmythzoneminder.dll

mkdir -p platforms
mkdir -p sqldrivers
cp qwindows.dll $buildPath"/install/bin/platforms/windowplugin.dll"
cp qwindowsvistastyle.dll $buildPath"/install/bin/platforms/qwindowsvistastyle.dll"
cp qwindowsvistastyle.dll $buildPath"/install/bin/platforms/libtasn1-6.dll"
cp qsqlmysql.dll $buildPath"/install/bin/sqldrivers/qsqlmysql.dll"
cd ..

find . -name \*.a -exec cp {} \lib \;
find . -name \*.lib -exec cp {} \lib \;

zip -rq MythTv_Windows.zip *

echo "Done"
