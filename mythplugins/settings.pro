#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.18

INCLUDEPATH += $${PREFIX}/include
#INCLUDEPATH += /usr/include/cdda
#INCLUDEPATH += /usr/include/FLAC

LIBS *= -L$${PREFIX}/lib

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"

# Remove for x86-64
DEFINES += HAVE_MMX

release {
    QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
    macx {
        # Don't use -O3, it causes some Qt moc methods to go missing
        QMAKE_CXXFLAGS_RELEASE = -O2
    }
    QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
}

macx {
    DEFINES -= HAVE_MMX
}

