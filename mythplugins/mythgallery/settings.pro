#CONFIG += debug
CONFIG += release

INCLUDEPATH += /usr/local/include

DEFINES += _GNU_SOURCE
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions -fno-rtti -fno-exceptions
}
