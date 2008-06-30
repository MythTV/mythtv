include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs

LIBS += -lmyth-$${LIBVERSION} -L../../libs/libmyth
LIBS += -lmythdb-$${LIBVERSION} -L../../libs/libmythdb
macx:LIBS += -lmythui-$${LIBVERSION} -L../../libs/libmythui
macx:LIBS += -lmythupnp-$${LIBVERSION} -L../../libs/libmythupnp

# Input
SOURCES += filter_kerneldeint.c
