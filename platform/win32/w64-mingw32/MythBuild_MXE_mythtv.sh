#!/bin/bash

export buildPath=$PWD"/build"
export PATH=$buildPath"/mxe/usr/bin":$PATH
export qt5=$buildPath"/mxe/usr/i686-w64-mingw32.shared/qt5"
export in1=$buildPath"/mxe/usr/i686-w64-mingw32.shared"

git clone https://github.com/MythTV/mythtv.git

cd $buildPath/mythtv
#git clean -xfd .
#git fetch me
#git reset --hard me/master
#cp platform/win32/w64-mingw32/Patches/libexiv2.patch libexiv2.patch
#git apply libexiv2.patch -v
#rm libexiv2.patch

echo "Compiling mythtv"
cd $buildPath/mythtv/mythtv
./configure --prefix="$buildPath/install" --enable-cross-compile --cross-prefix=i686-w64-mingw32.shared- --target_os=mingw32 --arch=x86 --cpu=pentium3 --qmake=$qt5/bin/qmake --extra-cflags=-I$in1/include-I/home/ubuntu/Desktop/build/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++/i686-w64-mingw32.shared --extra-ldflags=-L$in1/lib --disable-lirc --disable-hdhomerun --disable-firewire --disable-vdpau  --disable-nvdec --disable-dxva2 --enable-libmp3lame --enable-libx264 --enable-libx265 --enable-libxvid --enable-libvpx --disable-w32threads

make -j$(nproc)

cp $buildPath/mythtv/mythtv/external/FFmpeg/ffmpeg_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffmpeg.exe
cp $buildPath/mythtv/mythtv/external/FFmpeg/ffprobe_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffprobe.exe
make install
cd ..

chmod -R 777 $buildPath/install
cd mythplugins

./configure --prefix="$buildPath/install" -cross-prefix=i686-w64-mingw32.shared- --disable-mytharchive
make -j$(nproc) install
cd ..

cp -R $buildPath/mythtv/platform/win32/w64-mingw32/Installer/. $buildPath/install/
cp -R $buildPath/mythtv/mythtv/src/COPYING $buildPath/install/COPYING
cp -R $buildPath/themes/ $buildPath/install/share/mythtv/

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

zip -r MythTv_Windows.zip *

echo "Done"
