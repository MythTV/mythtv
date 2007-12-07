include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs

LIBS += -lmyth-$${LIBVERSION} -L../../libs/libmyth

# Input
SOURCES += filter_kerneldeint.c
