# Common project modifications to change the generated target.

mingw {
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.dll.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}


        # Windows doesn't have a nice variable like LD_LIBRARY_PATH,
        # which means make install would be broken without extra steps.
        # As a workaround, we store dlls with exes. Also improves debugging!
        #
        target.path = $${PREFIX}/bin
    }
}
