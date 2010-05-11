CONFIG += $$CCONFIG

LIBVERSION = 0.23

INCLUDEPATH += $${SYSROOT}$${PREFIX}/include
#INCLUDEPATH += /usr/include/cdda
#INCLUDEPATH += /usr/include/FLAC

LIBS *= -L$${SYSROOT}$${PREFIX}/$${LIBDIRNAME}

isEmpty(TARGET_OS) : win32 {
    CONFIG += mingw
    DEFINES += USING_MINGW
    # Qt4 creates separate compile directories by default. This disables:
    CONFIG -= debug_and_release debug_and_release_target
    # Some shared libs we depend on are installed here:
    LIBS += -L/bin
}

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"

INCLUDEPATH += $$CONFIG_INCLUDEPATH

# Prevent building .app bundles everywhere.
macx:CONFIG += console

# figure out compile flags based on qmake info

QMAKE_CXXFLAGS += $$ARCHFLAGS
QMAKE_CXXFLAGS += $$CONFIG_DIRECTFB_CXXFLAGS
QMAKE_CXXFLAGS_SHLIB = -DPIC -fPIC
QMAKE_CXXFLAGS += $$ECXXFLAGS

profile:CONFIG += release

QMAKE_CXXFLAGS_RELEASE = $$OPTFLAGS -fomit-frame-pointer
release:contains( TARGET_ARCH_POWERPC, yes ) {
    QMAKE_CXXFLAGS_RELEASE = $$OPTFLAGS
    # Auto-inlining causes some Qt moc methods to go missing
    macx:QMAKE_CXXFLAGS_RELEASE += -fno-inline-functions
}
QMAKE_CXXFLAGS_RELEASE += $$PROFILEFLAGS

QMAKE_CFLAGS += $$ARCHFLAGS
QMAKE_CFLAGS_SHLIB = -DPIC -fPIC
QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
QMAKE_CFLAGS += $$ECFLAGS

# figure out defines 

DEFINES += $$CONFIG_DEFINES
DEFINES += _FILE_OFFSET_BITS=64

# construct linking path

LOCAL_LIBDIR_X11 =
!isEmpty( QMAKE_LIBDIR_X11 ) {
    LOCAL_LIBDIR_X11 = -L$$QMAKE_LIBDIR_X11
}
QMAKE_LIBDIR_X11 = 

EXTRA_LIBS += -lfreetype -lmp3lame
EXTRA_LIBS += $$CONFIG_AUDIO_ALSA_LIBS
EXTRA_LIBS += $$CONFIG_AUDIO_JACK_LIBS
EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS
EXTRA_LIBS += $$CONFIG_DIRECTFB_LIBS

EXTRA_LIBS += $$LOCAL_LIBDIR_X11
EXTRA_LIBS += $$CONFIG_XV_LIBS
EXTRA_LIBS += $$CONFIG_XVMC_LIBS
EXTRA_LIBS += $$CONFIG_OPENGL_VSYNC_LIBS

LIRC_LIBS = $$CONFIG_LIRC_LIBS
