include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

LIBS += -lmyth-$${LIBVERSION} -L../../libs/libmyth
LIBS += -lmythbase-$${LIBVERSION} -L../../libs/libmythbase
macx:LIBS += -lmythui-$${LIBVERSION} -L../../libs/libmythui
macx:LIBS += -lmythupnp-$${LIBVERSION} -L../../libs/libmythupnp
mingw:LIBS += -lpthread

# Input
SOURCES += filter_kerneldeint.c
