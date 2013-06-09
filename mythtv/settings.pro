include ( config.mak )

CONFIG += $$CCONFIG

defineReplace(avLibName) {
        NAME = $$1

        major = \$\${lib$${NAME}_VERSION_MAJOR}
        eval(LIBVERSION = $$major)

        temp = $$SLIBNAME_WITH_MAJOR_QT
        temp = $$replace(temp, FULLNAME, $$NAME)
        temp = $$replace(temp, NAME,     $$NAME)
        temp = $$replace(temp, LIBMAJOR, $$LIBVERSION)

        return($$temp)
}

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

LIBVERSION = 0.27
VERSION = 0.27.0

isEmpty(TARGET_OS) : win32 {
    CONFIG += mingw
    DEFINES += WIN32 USING_MINGW WIN32_LEAN_AND_MEAN NOMINMAX
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
# For dependencies on Myth library filenames in POST_TARGETDEPS
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
INCLUDEPATH += $$SRC_PATH_BARE/external/zeromq/include
INCLUDEPATH += $$SRC_PATH_BARE/external/nzmqt/include/nzmqt
INCLUDEPATH += $$SRC_PATH_BARE/external/qjson/include

# remove warn_{on|off} from CONFIG since we set it in our CFLAGS
CONFIG -= warn_on warn_off

# set empty RELEASE and DEBUG flags
QMAKE_CFLAGS_DEBUG     =
QMAKE_CFLAGS_RELEASE   =
QMAKE_CXXFLAGS_DEBUG   =
QMAKE_CXXFLAGS_RELEASE =

# remove -fPIC since we handle it in configure
QMAKE_CFLAGS_SHLIB      -= -fPIC
QMAKE_CFLAGS_STATIC_LIB -= -fPIC
QMAKE_LFLAGS_SHLIB      -= -fPIC

# remove -D_ISOC99_SOURCE -D_POSIX_C_SOURCE=200112 from C++ preprocessor flgas
CXX_PP_FLAGS  = $$CPPFLAGS
CXX_PP_FLAGS -= -D_ISOC99_SOURCE -D_POSIX_C_SOURCE=200112

# Globals in static libraries need special treatment on OS X
macx: QMAKE_CFLAGS_STATIC_LIB += -fno-common

# figure out compile flags based on qmake info
# qmake 4.8.2 & 4.8.3 messes up OSX "-arch i386 -arch x86_64"
# clang 3.0 on Linux does not like duplicate arguments.
macx {
QMAKE_CFLAGS   += $$CPPFLAGS   $$CFLAGS
QMAKE_CXXFLAGS += $$CXXPPFLAGS $$ECXXFLAGS
} else {
QMAKE_CFLAGS   *= $$CPPFLAGS   $$CFLAGS
QMAKE_CXXFLAGS *= $$CXXPPFLAGS $$ECXXFLAGS
}

profile:CONFIG += debug

# Allow compilation with Qt Embedded, if Qt is compiled without "-fno-rtti"
QMAKE_CXXFLAGS -= -fno-exceptions -fno-rtti

release:contains( ARCH_POWERPC, yes ) {
    # Auto-inlining causes some Qt moc methods to go missing
    macx:QMAKE_CXXFLAGS_RELEASE += -fno-inline-functions
}


# figure out defines
DEFINES += $$CONFIG_DEFINES
DEFINES += _GNU_SOURCE

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

!isEmpty( QMAKE_LIBDIR_QT ) {
    !macx {
        LATE_LIBS += "-L$$QMAKE_LIBDIR_QT"
        QMAKE_LIBDIR_QT = ""
    }
    macx:!qt_framework {
        LATE_LIBS += "-L$$QMAKE_LIBDIR_QT"
        QMAKE_LIBDIR_QT = ""
    }
}
EXTRA_LIBS  = $$EXTRALIBS

EXTRA_LIBS += $$FREETYPE_LIBS
EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS

EXTRA_LIBS += $$LOCAL_LIBDIR_OGL
EXTRA_LIBS += $$LOCAL_LIBDIR_X11
EXTRA_LIBS += $$CONFIG_OPENGL_LIBS
EXTRA_LIBS += -L$$SRC_PATH_BARE/external/zeromq/src/.libs -lmythzmq
EXTRA_LIBS += -L$$SRC_PATH_BARE/external/nzmqt/src -lmythnzmqt
EXTRA_LIBS += -L$$SRC_PATH_BARE/external/qjson/lib -lmythqjson

macx {
    using_firewire:using_backend:EXTRA_LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
    QMAKE_LFLAGS_SONAME  = -Wl,-install_name,@rpath/
}

macx {
    _RPATH_="-rpath,"
} else {
    _RPATH_="-rpath="
}
