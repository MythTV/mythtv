include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc

# Input
SOURCES += main.cpp

#The following line was inserted by qt3to4
QT += xml qt3support
