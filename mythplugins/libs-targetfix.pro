# Common project modifications to change the generated target.

mingw {
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.so.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}
    }
}

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
