# Trigger rebuilds if installed MythTV libs have changed


# Stolen from mythtv/settings.pro:
isEmpty(TARGET_OS) : win32     : QMAKE_EXTENSION_SHLIB=dll
contains(TARGET_OS, CYGWIN)    : QMAKE_EXTENSION_SHLIB=dll.a
isEmpty(QMAKE_EXTENSION_SHLIB) : QMAKE_EXTENSION_SHLIB=so
isEmpty(QMAKE_EXTENSION_LIB)   : QMAKE_EXTENSION_LIB=a
MYTH_SHLIB_EXT=$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
MYTH_LIB_EXT  =$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}


DEPLIBS = $${LIBDIR}

# On Windows, dlls were installed with exes:
mingw : DEPLIBS = $${PREFIX}/bin

TARGETDEPS += $${DEPLIBS}/libmyth-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${DEPLIBS}/libmythdb-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${DEPLIBS}/libmythui-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${DEPLIBS}/libmythupnp-$${MYTH_SHLIB_EXT}
TARGETDEPS += $${DEPLIBS}/libmythavutil.$${libavutil_VERSION_MAJOR}$${SLIBSUF}
TARGETDEPS += $${DEPLIBS}/libmythavcodec.$${libavcodec_VERSION_MAJOR}$${SLIBSUF}
TARGETDEPS += $${DEPLIBS}/libmythavcore.$${libavcore_VERSION_MAJOR}$${SLIBSUF}
TARGETDEPS += $${DEPLIBS}/libmythavformat.$${libavformat_VERSION_MAJOR}$${SLIBSUF}
