include ( ../settings.pro )
include ( config.pro )

!exists( config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythphone
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = webcam-ui.xml phone-ui.xml images/*.png

INSTALLS += uifiles

LIBS += 

HEADERS += config.h PhoneSettings.h webcam.h phoneui.h sipfsm.h directory.h dbcheck.h rtp.h g711.h vxml.h tts.h wavfile.h h263.h tone.h sipstack.h md5digest.h

SOURCES += main.cpp webcam.cpp PhoneSettings.cpp phoneui.cpp sipfsm.cpp directory.cpp dbcheck.cpp rtp.cpp g711.cpp g711tabs.cpp vxml.cpp tts.h wavfile.cpp h263.cpp tone.cpp sipstack.cpp md5digest.cpp


