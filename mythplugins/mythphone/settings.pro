#CONFIG += debug
CONFIG += release

PREFIX = /usr/local
#FESTIVALDIR = ../../festival/festival/
#SPEECHTOOLSDIR = ../../festival/speech_tools/
MYTHLIBDIR = ../../mythtv/libs/

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include
INCLUDEPATH += $${MYTHLIBDIR}
#INCLUDEPATH += $${FESTIVALDIR}/src/include
#INCLUDEPATH += $${SPEECHTOOLSDIR}/include

#LIBS += -L$${FESTIVALDIR}/src/lib
#LIBS += -L$${SPEECHTOOLSDIR}/lib
#LIBS += -lFestival -lestools -lestbase -leststring -ltermcap
#LIBS += -L/home/paul/Build/mythtv15/mythphone-0.15/mythphone/g729
#LIBS += -lva_g729a

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += _REENTRANT
DEFINES += USE_PTHREADS
#DEFINES += FESTIVAL_HOME=\"$${FESTIVALDIR}\"
release {
        QMAKE_CXXFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = -Wno-unused -O3 -march=pentiumpro -fomit-frame-pointer
}

