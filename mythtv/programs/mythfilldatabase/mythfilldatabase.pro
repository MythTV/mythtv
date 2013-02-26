include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

INCLUDEPATH += ../../libs/libmythtv/mpeg
DEPENDPATH  += ../../libs/libmythtv/mpeg

# Input
HEADERS += filldata.h   channeldata.h
HEADERS += xmltvparser.h
HEADERS += fillutil.h   commandlineparser.h
SOURCES += filldata.cpp channeldata.cpp
SOURCES += xmltvparser.cpp fillutil.cpp
SOURCES += main.cpp     commandlineparser.cpp
