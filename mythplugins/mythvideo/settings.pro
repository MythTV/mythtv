#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include
INCLUDEPATH *= $${PREFIX}/include/mythtv

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}
