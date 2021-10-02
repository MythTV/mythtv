echo "Tested on ubuntu 2004. Run this file to build mythtv for Windows"

sudo apt-get --assume-yes install git
sudo apt-get --assume-yes install gcc
sudo apt-get --assume-yes install g++
sudo apt-get --assume-yes install wget
sudo apt-get --assume-yes install python3
sudo apt-get --assume-yes install perl
sudo apt-get --assume-yes install bzip2
sudo apt-get --assume-yes install xy-utils
sudo apt-get --assume-yes install lzip
sudo apt-get --assume-yes install unzip
sudo apt-get --assume-yes install libssl-dev
sudo apt-get --assume-yes install p7zip
sudo apt-get --assume-yes install make
sudo apt-get --assume-yes install autoconf
sudo apt-get --assume-yes install automake
sudo apt-get --assume-yes install bison
sudo apt-get --assume-yes install flex
sudo apt-get --assume-yes install autopoint
sudo apt-get --assume-yes install gperf
sudo apt-get --assume-yes install libtool
sudo apt-get --assume-yes install libtool-bin
sudo apt-get --assume-yes install ruby
sudo apt-get --assume-yes install intltool
sudo apt-get --assume-yes install p7zip-full
sudo apt-get --assume-yes install python

export buildPath=$PWD"/build"
mkdir -p $buildPath
cd $buildPath
mkdir -p install
mkdir -p install/bin
mkdir -p install/bin/plugins
mkdir -p themes

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

git clone https://github.com/MythTV/mythtv.git

git clone https://github.com/mxe/mxe.git

echo "Add SQL to QT"
sed -i 's/-no-sql-mysql /\//g' $buildPath/mxe/src/qt.mk

sudo apt-get --assume-yes install pkg-config

cd mxe
sudo make cc MXE_PLUGIN_DIRS=plugins/gcc8 MXE_TARGETS='i686-w64-mingw32.shared' vulkan-loader vulkan-headers qt5 nasm yasm libsamplerate taglib zlib gnutls mman-win32 pthreads libxml2 libdvdcss x264 x265 lame libass qtwebkit xvidcore libvpx vorbis flac
cd ..

find . -name \*.dll -exec cp {} \install/bin \;

export PATH=$buildPath"/mxe/usr/bin":$PATH
export qt5=$buildPath"/mxe/usr/i686-w64-mingw32.shared/qt5"
export in1=$buildPath"/mxe/usr/i686-w64-mingw32.shared"
sudo chmod -R 777 $buildPath/mxe

echo -e "#define RTLD_LAZY 0 \n#define HAVE_DVDCSS_DVDCSS_H" | sudo -E tee $buildPath/mxe/usr/i686-w64-mingw32.shared/include/dlfcn.h

sudo -E sed -i 's/#define GetUserName __MINGW_NAME_AW(GetUserName)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h
sudo -E sed -i 's/#define CopyFile __MINGW_NAME_AW(CopyFile)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h
sudo -E sed -i 's/#define MoveFile __MINGW_NAME_AW(MoveFile)//g' $buildPath/mxe/usr/i686-w64-mingw32.shared/include/winbase.h

sudo apt-get --assume-yes remove yasm

sudo cp $buildPath/mxe/usr/x86_64-pc-linux-gnu/bin/yasm $buildPath//mxe/usr/bin/yasm
sudo cp $buildPath/mxe/usr/bin/i686-w64-mingw32.shared-pkg-config $buildPath//mxe/usr/bin/pkg-config
sudo cp -R $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++ $buildPath/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include

cp $buildPath/mythtv/platform/win32/w64-mingw32/Patches/libexiv2.patch $buildPath/mythtv/libexiv2.patch
cd mythtv
git apply libexiv2.patch -v
rm libexiv2.patch
cd ..

echo "Compiling libudfread"
git clone https://code.videolan.org/videolan/libudfread.git
cd libudfread
./bootstrap
./configure --prefix=$buildPath/mxe/usr/i686-w64-mingw32.shared --host=i686-w64-mingw32.shared
make -j$(nproc)
sudo make install
cd ..

git clone https://code.videolan.org/videolan/libbluray.git
cd $buildPath/libbluray
git submodule update --init

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
git clone https://gitlab.com/soundtouch/soundtouch.git
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


sudo apt-get --assume-yes remove pkg-config

echo "Compiling mythtv"
cd $buildPath/mythtv/mythtv
./configure --prefix="$buildPath/install" --enable-cross-compile --cross-prefix=i686-w64-mingw32.shared- --target_os=mingw32 --arch=x86 --cpu=pentium3 --qmake=$qt5/bin/qmake --extra-cflags=-I$in1/include-I/home/ubuntu/Desktop/build/mxe/usr/lib/gcc/i686-w64-mingw32.shared/8.4.0/include/c++/i686-w64-mingw32.shared --extra-ldflags=-L$in1/lib --disable-lirc --disable-hdhomerun --disable-firewire --disable-ivtv --disable-vdpau  --disable-nvdec --disable-dxva2 --enable-libmp3lame --enable-libx264 --enable-libx265 --enable-libxvid --enable-libvpx --disable-w32threads

make -j$(nproc)

sudo -E cp $buildPath/mythtv/mythtv/external/FFmpeg/ffmpeg_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffmpeg.exe
sudo -E cp $buildPath/mythtv/mythtv/external/FFmpeg/ffprobe_g.exe $buildPath/mythtv/mythtv/external/FFmpeg/mythffprobe.exe
sudo -E make install
cd ..

sudo chmod -R 777 $buildPath/install
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
