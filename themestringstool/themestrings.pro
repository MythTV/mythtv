include ( ../mythtv/settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc

# Input
SOURCES += main.cpp

QT += xml
