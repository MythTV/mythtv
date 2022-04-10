include ( ../mythtv/settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc

# Input
SOURCES += themestrings.cpp

QT += xml
QT += widgets
