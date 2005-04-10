#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.18

INCLUDEPATH += $${PREFIX}/include

LIBS *= -L$${PREFIX}/lib

DEFINES += _GNU_SOURCE
release {
    QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
    macx {
        # Don't use -O3, it causes some Qt moc methods to go missing
        QMAKE_CXXFLAGS_RELEASE = -O2
    }
    QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
}

