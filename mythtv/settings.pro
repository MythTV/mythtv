#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

INCLUDEPATH += $${PREFIX}/include

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"

release {
        DEFINES += MMX
        QMAKE_CXXFLAGS_RELEASE = -O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions -fno-rtti -fno-exceptions
}

