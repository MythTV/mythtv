#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

INCLUDEPATH += $${PREFIX}/include
#INCLUDEPATH += /usr/include/cdda
#INCLUDEPATH += /usr/include/FLAC

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += HAVE_MMX
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}

macx {
    DEFINES -= HAVE_MMX
}

