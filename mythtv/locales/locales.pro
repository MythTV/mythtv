include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

locales.path = $${PREFIX}/share/mythtv/locales/
locales.files = *.xml

INSTALLS += locales

# Input
SOURCES += dummy.c
