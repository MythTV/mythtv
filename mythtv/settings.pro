#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

LIBVERSION = 0.9

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include

DEFINES += _GNU_SOURCE
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += PREFIX=\"$${PREFIX}\"

release {
        DEFINES += MMX
        QMAKE_CXXFLAGS_RELEASE = -O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations
}

