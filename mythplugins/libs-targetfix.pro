# Common project modifications to change the generated target.

mingw {
    contains( TEMPLATE, lib ) : !static {

        # Qt under Linux/Unix/Mac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults   libBlah.a and    Blah.so.
        #
        # So that our modules have the same filenames as on Unix, override:
        #
        TARGET = lib$${TARGET}
    }
}

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

# Trigger rebuilds if installed MythTV includes have changed
DEPENDPATH = $${INCLUDEPATH}

# Trigger rebuilds if installed MythTV libs have changed
include (../../targetdep.pro)
