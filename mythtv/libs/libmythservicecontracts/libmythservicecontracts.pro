include ( ../../settings.pro )

using_qtscript: QT += script

TEMPLATE = lib
TARGET = mythservicecontracts-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += SERVICE_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

LIBS += -L../libmythbase -lmythbase-$${LIBVERSION}

INCLUDEPATH += ..

# Input

HEADERS += serviceexp.h service.h datacontracthelper.h

HEADERS += services/frontendServices.h
HEADERS += services/rttiServices.h

HEADERS += datacontracts/enum.h                  datacontracts/enumItem.h
HEADERS += datacontracts/frontendActionList.h    datacontracts/frontendStatus.h

SOURCES += service.cpp

inc.path = $${PREFIX}/include/mythtv/libmythservicecontracts/
inc.files = serviceexp.h service.h datacontracthelper.h

incServices.files += services/frontendServices.h
incServices.files += services/rttiServices.h

incDatacontracts.files += datacontracts/frontendStatus.h      datacontracts/frontendActionList.h
incDatacontracts.files += datacontracts/enum.h                datacontracts/enumItem.h

INSTALLS += inc incServices incDatacontracts

macx {
    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
