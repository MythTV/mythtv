INCLUDEPATH += ../../libs/
LIBS += -L../../libs/libmyth -L/usr/local/lib

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmyth

# Input
SOURCES += filldata.cpp
