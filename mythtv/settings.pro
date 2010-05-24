include ( config.mak )

CONFIG += $$CCONFIG

#check QT major version
contains(QT_MAJOR_VERSION, 3) {
        error("Must build against Qt4")
}

# Where binaries, includes and runtime assets are installed by 'make install'
isEmpty( PREFIX ) {
    PREFIX = /usr/local
}
# Where the binaries actually locate the assets/filters/plugins at runtime
isEmpty( RUNPREFIX ) {
    RUNPREFIX = $$PREFIX
}
# Alternate library dir for OSes and packagers (e.g. lib64)
isEmpty( LIBDIRNAME ) {
    LIBDIRNAME = lib
}
# Where libraries, plugins and filters are installed
isEmpty( LIBDIR ) {
    LIBDIR = $${RUNPREFIX}/$${LIBDIRNAME}
}

LIBVERSION = 0.23
VERSION = 0.23.0

isEmpty(TARGET_OS) : win32 {
    CONFIG += mingw
    DEFINES += WIN32 USING_MINGW
    DEFINES -= UNICODE
    QMAKE_EXTENSION_SHLIB = dll
    VERSION =
    CONFIG_OPENGL_LIBS =
    # Qt4 creates separate compile directories by default. This disables:
    CONFIG -= debug_and_release debug_and_release_target
    # win32-packager.pl builds Qt under DOS, but MythTV is built in MinGW.
    # This corrects the moc tool path from a DOS-style to a unix style:
    QMAKE_MOC = $$[QT_INSTALL_BINS]/moc
}

# if CYGWIN compile, set up flag in CONFIG
contains(TARGET_OS, CYGWIN) {
    CONFIG += cygwin
    QMAKE_EXTENSION_SHLIB=dll.a
    DEFINES += CONFIG_CYGWIN
}

isEmpty(QMAKE_EXTENSION_SHLIB) {
    QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
    QMAKE_EXTENSION_LIB=a
}
# For dependencies on Myth library filenames in TARGETDEPS
MYTH_SHLIB_EXT=$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
MYTH_LIB_EXT  =$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}

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

INCLUDEPATH += $$unique(CONFIG_INCLUDEPATH)

# remove warn_{on|off} from CONFIG since we set it in our CFLAGS
CONFIG -= warn_on warn_off

# set empty RELEASE and DEBUG flags
QMAKE_CFLAGS_DEBUG     =
QMAKE_CFLAGS_RELEASE   =
QMAKE_CXXFLAGS_DEBUG   =
QMAKE_CXXFLAGS_RELEASE =

# figure out compile flags based on qmake info
QMAKE_CFLAGS   += $$OPTFLAGS $$PROFILEFLAGS $$CFLAGS
QMAKE_CXXFLAGS += $$OPTFLAGS $$PROFILEFLAGS $$ECXXFLAGS

profile:CONFIG += debug

QMAKE_CXXFLAGS += $$CONFIG_DIRECTFB_CXXFLAGS

QMAKE_CFLAGS_SHLIB   = -DPIC -fPIC
QMAKE_CXXFLAGS_SHLIB = -DPIC -fPIC

# Allow compilation with Qt Embedded, if Qt is compiled without "-fno-rtti"
QMAKE_CXXFLAGS -= -fno-exceptions -fno-rtti

release:contains( ARCH_POWERPC, yes ) {
    # Auto-inlining causes some Qt moc methods to go missing
    macx:QMAKE_CXXFLAGS_RELEASE += -fno-inline-functions
}


# figure out defines
DEFINES += $$CONFIG_DEFINES
DEFINES += _GNU_SOURCE
DEFINES += _FILE_OFFSET_BITS=64

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

OSS_LIBS = $$CONFiG_AUDIO_OSS_LIBS
ALSA_LIBS = $$CONFIG_AUDIO_ALSA_LIBS
JACK_LIBS = $$CONFIG_AUDIO_JACK_LIBS
PULSE_LIBS = $$CONFIG_AUDIO_PULSE_LIBS

EXTRA_LIBS += $$FREETYPE_LIBS
EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS
EXTRA_LIBS += $$CONFIG_DIRECTFB_LIBS

EXTRA_LIBS += $$LOCAL_LIBDIR_OGL
EXTRA_LIBS += $$LOCAL_LIBDIR_X11
EXTRA_LIBS += $$CONFIG_X11_LIBS
EXTRA_LIBS += $$CONFIG_XV_LIBS
EXTRA_LIBS += $$CONFIG_XRANDR_LIBS
EXTRA_LIBS += $$CONFIG_XVMC_LIBS
EXTRA_LIBS += $$CONFIG_OPENGL_LIBS
EXTRA_LIBS += $$FRIBIDI_LIBS
using_pulse:EXTRA_LIBS += $$PULSE_LIBS

LIRC_LIBS = $$CONFIG_LIRC_LIBS

LATE_LIBS += "-L$$QMAKE_LIBDIR_QT"
QMAKE_LIBDIR_QT = ""
