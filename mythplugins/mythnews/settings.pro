#CONFIG += debug
CONFIG += release

PREFIX = /usr/local
# FOR Debian
#PREFIX = /usr

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include

LIBPATH += $${PREFIX}/lib

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
	QMAKE_CXXFLAGS_RELEASE = -O2 -fomit-frame-pointer
}	
