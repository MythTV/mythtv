CONFIG += debug
#CONFIG += release

!exists( config.pro ) {
    error(Missing config.pro: please run the configure script)
}
include ( config.pro )

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.20

INCLUDEPATH += $${PREFIX}/include

LIBS *= -L$${PREFIX}/lib

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += __STDC_CONSTANT_MACROS
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}
debug {
        QMAKE_CXXFLAGS_DEBUG += -DMFD_DEBUG_BUILD
}
