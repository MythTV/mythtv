#CONFIG += debug
CONFIG += release
LIBVERSION = 0.18

isEmpty( PREFIX ) {
    PREFIX = /usr/
}


INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= $${PREFIX}/include/mythtv

DEFINES += _GNU_SOURCE
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
    macx {
        # Don't use -O3, it causes some Qt moc methods to go missing
        QMAKE_CXXFLAGS_RELEASE = -O2
    }
}
