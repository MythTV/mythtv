#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

LIBVERSION = 0.13

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}

