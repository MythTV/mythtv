include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv
DEPENDPATH  += ../../libs/libmythtv

SOURCES += filter_postprocess.c

# Lots of symbols like pp_free_context, pp_free_mode, pp_get_context, pp_help
# are used but not defined, which sometimes prevents linking on OS X.
macx:LIBS += -undefined define_a_way
