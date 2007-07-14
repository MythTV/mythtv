include ( ../filter-common.pro )
include ( ../filter-avcodec.pro)

INCLUDEPATH += ../../libs/libmythtv ../..

# Input
SOURCES += pullup.c filter_ivtc.c
