# There is a circular library dep between libmyth and libmythui/libmythupnp
# libmyth is built last, so the other two libraries need to either know,
# or ignore, the missing symbols that will be defined in libmyth.

macx:QMAKE_LFLAGS_SHLIB += -undefined warning

mingw {
    # Worked around by building libmyth first,
    # with dlltool faking the symbols from the other two libs
    LIBS += -L../libmyth -lmyth-$$LIBVERSION
}

#unix {
    # Search for --as-needed in the link paths, and remove it
#}
