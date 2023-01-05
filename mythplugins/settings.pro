CONFIG += $$CCONFIG
CONFIG += c++17

# Make sure all the Qt header files are marked as system headers
QMAKE_DEFAULT_INCDIRS += $$[QT_INSTALL_HEADERS]
INCLUDEPATH += $$[QT_INSTALL_HEADERS]

include(settings2.pro)

MY_INSTALL_INCLUDE = $${SYSROOT}$${PREFIX}/include
!contains(MY_INSTALL_INCLUDE, /usr/include$) {
    INCLUDEPATH += $${SYSROOT}$${PREFIX}/include
}

LIBS *= -L$${SYSROOT}$${PREFIX}/$${LIBDIRNAME}

isEmpty(TARGET_OS) : win32 {
    CONFIG += mingw
    DEFINES += USING_MINGW WIN32_LEAN_AND_MEAN NOMINMAX
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
QMAKE_CXXFLAGS += $$CXXFLAGS $$ECXXFLAGS

profile:!win32:!macx:CONFIG += debug

QMAKE_CXXFLAGS_RELEASE = $$OPTFLAGS -fomit-frame-pointer
QMAKE_CXXFLAGS_RELEASE += $$PROFILEFLAGS

QMAKE_CFLAGS += $$ARCHFLAGS
QMAKE_CFLAGS_SHLIB = -DPIC -fPIC
QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
QMAKE_CFLAGS += $$CFLAGS

# figure out defines

DEFINES += $$CONFIG_DEFINES
DEFINES += _FILE_OFFSET_BITS=64

# construct linking path

LOCAL_LIBDIR_X11 =
!isEmpty( QMAKE_LIBDIR_X11 ) {
    LOCAL_LIBDIR_X11 = -L$$QMAKE_LIBDIR_X11
}
QMAKE_LIBDIR_X11 =

EXTRA_LIBS += $$EXTRALIBS
EXTRA_LIBS += $$FREETYPE_LIBS
contains(CONFIG_LIBMP3LAME, "yes") {
  EXTRA_LIBS += -lmp3lame
}
EXTRA_LIBS += $$CONFIG_AUDIO_ALSA_LIBS
EXTRA_LIBS += $$CONFIG_AUDIO_JACK_LIBS
EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS
EXTRA_LIBS += $$CONFIG_DIRECTFB_LIBS

EXTRA_LIBS += $$LOCAL_LIBDIR_X11
EXTRA_LIBS += $$CONFIG_XV_LIBS
EXTRA_LIBS += $$CONFIG_XVMC_LIBS
EXTRA_LIBS += $$CONFIG_OPENGL_VSYNC_LIBS

LIRC_LIBS = $$CONFIG_LIRC_LIBS

macx:using_firewire:using_backend:EXTRA_LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices

#
# Stash generated objects in a subdirectory
#
win32 {
    CONFIG(debug, debug|release) {
        MOC_DIR         = debug/moc
        OBJECTS_DIR     = debug/obj
    } else {
        MOC_DIR         = release/moc
        OBJECTS_DIR     = release/obj
    }
} else {
    MOC_DIR         = moc
    OBJECTS_DIR     = obj
}

using_compdb:contains(CC, clang) {
    QMAKE_CFLAGS += "-MJ $@.json"
    QMAKE_CXXFLAGS += "-MJ $@.json"
}
