include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( config.pro )

!exists( config.pro ) {
    error(Missing config.pro: please run the configure script)
}

DEFINES += _RENTRANT
DEFINES += USE_PTHREADS
DEFINES += WAV49

QMAKE_CXXFLAGS_RELEASE += -Wno-unused
QMAKE_CFLAGS_RELEASE += -Wno-unused

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythphone
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = webcam-ui.xml phone-ui.xml images/*.png

INSTALLS += uifiles

LIBS += 

HEADERS += config.h PhoneSettings.h webcam.h phoneui.h sipfsm.h directory.h dbcheck.h rtp.h g711.h vxml.h tts.h wavfile.h h263.h tone.h sipstack.h md5digest.h gsm/proto.h gsm/unproto.h gsm/config.h gsm/private.h gsm/gsm.h gsm/k6opt.h dtmffilter.h audiodrv.h

SOURCES += main.cpp webcam.cpp PhoneSettings.cpp phoneui.cpp sipfsm.cpp directory.cpp dbcheck.cpp rtp.cpp g711.cpp g711tabs.cpp vxml.cpp tts.h wavfile.cpp h263.cpp tone.cpp sipstack.cpp md5digest.cpp gsm.cpp gsm/add.c gsm/code.c gsm/debug.c gsm/decode.c gsm/long_term.c gsm/lpc.c gsm/preprocess.c gsm/rpe.c gsm/gsm_destroy.c gsm/gsm_decode.c gsm/gsm_encode.c gsm/gsm_explode.c gsm/gsm_implode.c gsm/gsm_create.c gsm/gsm_print.c gsm/gsm_option.c gsm/short_term.c gsm/table.c dtmffilter.cpp audiodrv.cpp


