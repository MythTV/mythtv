CONFIG += debug
#CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

INCLUDEPATH += $${PREFIX}/include

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += _REENTRANT
DEFINES += USE_PTHREADS
DEFINES += WAV49

release {
        QMAKE_CXXFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
}

