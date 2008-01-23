# There is a circular library dep between libmyth and libmythui/libmythupnp
# libmyth is built last, so the other two libraries need to either know,
# or ignore, the missing symbols that will be defined in libmyth.

macx:QMAKE_LFLAGS_SHLIB += -undefined warning

mingw {
    LIBS += -L. -lmyth-bootstrap

    implib.target   = libmyth-bootstrap.a
    implib.depends  = ../libmyth/libmyth.def
    implib.commands = dlltool --input-def $$implib.depends         \
                              --dllname libmyth-$${LIBVERSION}.dll \
                              --output-lib $$implib.target -k

    QMAKE_EXTRA_WIN_TARGETS += implib
    POST_TARGETDEPS         += libmyth-bootstrap.a
}

#unix {
    # Search for --as-needed in the link paths, and remove it
#}
