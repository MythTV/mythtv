include ( ../settings.pro )

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfetchmovies
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmyth-$$LIBVERSION $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}

# Input
HEADERS += ddmovie.h

SOURCES += main.cpp ddmovie.cpp


