CONFIG += debug
#CONFIG += release

PREFIX = /usr/local
MYTHLIBDIR = ../../mythtv/libs/

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include
INCLUDEPATH += $${MYTHLIBDIR}

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += _REENTRANT
DEFINES += USE_PTHREADS

release {
        QMAKE_CXXFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
}

