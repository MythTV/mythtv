CONFIG += $$CCONFIG

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.20
VERSION = 0.20.0

# if CYGWIN compile, set up flag in CONFIG 
contains(TARGET_OS, CYGWIN) { 
    CONFIG += cygwin 
    QMAKE_EXTENSION_SHLIB=dll
    QMAKE_EXTENSION_CYGWIN = .a
    DEFINES += CONFIG_CYGWIN
}

# Die on the (common) case where OS X users inadvertently use Fink's
# Qt/X11 install instead of Qt/Mac. '
contains(CONFIG_DARWIN, yes) {
    !macx {
        message(You are building with Qt/X11 on the Mac platform.)
        message(Myth must be built with Qt/Mac instead.)
        message((Fink users cannot use Fink's Qt, it's the wrong one.))
        error(Unsupported configuration)
    }
}

# Prevent needlessly building .app bundles everywhere.
# Only MythFrontend, MythTV and MythTV-Setup really need to be bundles,
# and we do that in their Makefiles if CONFIG has mac_bundle
macx: CONFIG += console


INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH += $$CONFIG_INCLUDEPATH

# figure out compile flags based on qmake info

QMAKE_CXXFLAGS += $$ARCHFLAGS
QMAKE_CXXFLAGS += $$CONFIG_AUDIO_ARTS_CFLAGS
QMAKE_CXXFLAGS += $$CONFIG_DIRECTFB_CXXFLAGS
QMAKE_CXXFLAGS_SHLIB = -DPIC -fPIC

# Allow compilation with Qt Embedded, if Qt is compiled without "-fno-rtti"
QMAKE_CXXFLAGS -= -fno-exceptions -fno-rtti
macx:QMAKE_CXXFLAGS += -Wno-long-double

QMAKE_CXXFLAGS_RELEASE = $$OPTFLAGS $$ECXXFLAGS -fomit-frame-pointer
release:contains( TARGET_ARCH_POWERPC, yes ) {
    QMAKE_CXXFLAGS_RELEASE = $$OPTFLAGS $$ECXXFLAGS
    # Auto-inlining causes some Qt moc methods to go missing
    macx:QMAKE_CXXFLAGS_RELEASE += -fno-inline-functions
}

QMAKE_CFLAGS += $$ARCHFLAGS
QMAKE_CFLAGS_SHLIB = -DPIC -fPIC
QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -fomit-frame-pointer
QMAKE_CFLAGS += $$ECFLAGS

profile {
    QMAKE_CXXFLAGS_DEBUG = $${QMAKE_CXXFLAGS_RELEASE} $$PROFILEFLAGS
    QMAKE_CFLAGS_DEBUG = $${QMAKE_CXXFLAGS_RELEASE} $$PROFILEFLAGS
    CONFIG += debug    
}

QMAKE_CXXFLAGS_DEBUG += $$ECXXFLAGS

# figure out defines 

DEFINES += $$CONFIG_DEFINES
DEFINES += _GNU_SOURCE
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += PREFIX=\"$${PREFIX}\"
DEFINES += LIBDIR=\"$${LIBDIR}\"

# construct linking path

LOCAL_LIBDIR_X11 =
!isEmpty( QMAKE_LIBDIR_X11 ) {
    LOCAL_LIBDIR_X11 = -L$$QMAKE_LIBDIR_X11
}
QMAKE_LIBDIR_X11 = 

LOCAL_LIBDIR_OGL =
!isEmpty( QMAKE_LIBDIR_OPENGL ) {
    LOCAL_LIBDIR_OGL = -L$$QMAKE_LIBDIR_OPENGL
}
QMAKE_LIBDIR_OPENGL =

EXTRA_LIBS = $$FREETYPE_LIBS -lmp3lame
EXTRA_LIBS += $$CONFIG_AUDIO_OSS_LIBS
EXTRA_LIBS += $$CONFIG_AUDIO_ALSA_LIBS
EXTRA_LIBS += $$CONFIG_AUDIO_ARTS_LIBS
EXTRA_LIBS += $$CONFIG_AUDIO_JACK_LIBS
EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS
EXTRA_LIBS += $$CONFIG_DIRECTFB_LIBS

EXTRA_LIBS += $$LOCAL_LIBDIR_OGL
EXTRA_LIBS += $$LOCAL_LIBDIR_X11
EXTRA_LIBS += $$CONFIG_XV_LIBS
EXTRA_LIBS += $$CONFIG_XRANDR_LIBS
EXTRA_LIBS += $$CONFIG_XVMC_LIBS
EXTRA_LIBS += $$CONFIG_OPENGL_LIBS

LIRC_LIBS = $$CONFIG_LIRC_LIBS
