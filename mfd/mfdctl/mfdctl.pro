include ( ../settings.pro )

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}


TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS += target

TARGET = mfdctl
HEADERS += mfdctl.h
SOURCES += mfdctl.cpp main.cpp
LIBS += $$EXTRA_LIBS


