#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

# FOR Debian
#PREFIX = /usr

INCLUDEPATH += $${PREFIX}/include

LIBPATH += $${PREFIX}/lib

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
	QMAKE_CXXFLAGS_RELEASE = -O2 -fomit-frame-pointer
}	
