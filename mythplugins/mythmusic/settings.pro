#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include
INCLUDEPATH += /usr/include/cdda
INCLUDEPATH += /usr/include/FLAC

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}
