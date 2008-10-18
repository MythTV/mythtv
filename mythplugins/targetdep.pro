# Trigger rebuilds if installed MythTV libs have changed


# Stolen from mythtv/settings.pro:
isEmpty(TARGET_OS) : win32     : QMAKE_EXTENSION_SHLIB=dll
contains(TARGET_OS, CYGWIN)    : QMAKE_EXTENSION_SHLIB=dll.a
isEmpty(QMAKE_EXTENSION_SHLIB) : QMAKE_EXTENSION_SHLIB=so
isEmpty(QMAKE_EXTENSION_LIB)   : QMAKE_EXTENSION_LIB=a
MYTH_SHLIB_EXT=$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
MYTH_LIB_EXT  =$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}


# On Windows, dlls were installed with exes:
mingw : LIBDIR = $${PREFIX}/bin

TARGETDEPS += $${LIBDIR}/libmyth-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythdb-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythtv-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythui-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythupnp-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythavutil-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythavcodec-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${LIBDIR}/libmythavformat-$${MYTH_SHLIB_EXT}
