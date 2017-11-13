include ( ../../settings.pro )


HEADERS += hdhomerun.h  hdhomerun_os.h  hdhomerun_sock.h  hdhomerun_types.h
HEADERS += hdhomerun_channels.h  hdhomerun_channelscan.h  hdhomerun_control.h
HEADERS += hdhomerun_debug.h     hdhomerun_device.h       hdhomerun_device_selector.h
HEADERS += hdhomerun_discover.h  hdhomerun_pkt.h          hdhomerun_video.h 

SOURCES += hdhomerun_channels.c  hdhomerun_channelscan.c  hdhomerun_control.c
SOURCES += hdhomerun_debug.c     hdhomerun_device.c       hdhomerun_device_selector.c
SOURCES += hdhomerun_discover.c  hdhomerun_pkt.c          hdhomerun_video.c

QMAKE_CFLAGS += "-Wno-unused-parameter"
QMAKE_CXXFLAGS += "-Wno-unused-parameter"

unix {

    HEADERS += hdhomerun_os_posix.h
    SOURCES += hdhomerun_os_posix.c hdhomerun_sock_posix.c
    !android {
    LIBS += -lpthread

    !macx {
        LIBS += -lrt
    }
    }

}

win32 {

    HEADERS += hdhomerun_os_windows.h
    SOURCES += hdhomerun_os_windows.c hdhomerun_sock_windows.c
    SOURCES -= hdhomerun_os_posix.c
    LIBS += -lws2_32 -liphlpapi
    LIBS += $$EXTRALIBS

}

win32-msvc* {

    # needed for vcxproj
    QMAKE_CXXFLAGS += /TP

    # needed for nmake
    QMAKE_CFLAGS   += /TP

    DEFINES += DLL_EXPORT

    LIBS += -ladvapi32 -luser32
}

Makefile.lib | win32-msvc* {

    TEMPLATE = lib
    TARGET = mythhdhomerun-$$LIBVERSION
    CONFIG += thread dll
    CONFIG -= qt
    target.path = $${LIBDIR}
    INSTALLS = target

    QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)


    mingw {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.dll.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}

        # Windows doesnt have a nice variable like LD_LIBRARY_PATH,
        # which means make install would be broken without extra steps.
        # As a workaround, we store dlls with exes. Also improves debugging!
        #
        target.path = $${PREFIX}/bin

    }

}

Makefile.app {

    TERMPLATE = app
    TARGET = mythhdhomerun_config
    CONFIG += thread
    CONFIG -= qt
    target.path = $${PREFIX}/bin
    INSTALLS = target

    QMAKE_CLEAN += $(TARGET)

    # FIXME QMAKE_STRIP is empty sometimes, skip for now
    # unix:QMAKE_POST_LINK=$${QMAKE_STRIP} $(TARGET)
    
    SOURCES += hdhomerun_config.c

}

