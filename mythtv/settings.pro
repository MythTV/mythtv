CONFIG += debug
#CONFIG += release

PREFIX = /usr/local

LIBVERSION = 0.8

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"

DEFINES += EXTRA_LOCKING

release {
        DEFINES += MMX
        QMAKE_CXXFLAGS_RELEASE = -O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions 
}

