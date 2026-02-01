include (settings2.pro)
include ( config.mak )

CONFIG += $$CCONFIG
CONFIG += c17 c++17
CONFIG += no_qt_rpath

# Make sure all the Qt header files are marked as system headers
QMAKE_DEFAULT_INCDIRS += $$[QT_INSTALL_HEADERS]
INCLUDEPATH += $$[QT_INSTALL_HEADERS]

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
contains(QT_MAJOR_VERSION, 4) {
        error("Must build against Qt5 or higher")
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

# Windows...

win32 {

    VERSION =
    CONFIG_OPENGL_LIBS =

    # minGW Build Environment

    mingw {

        # Qt4 creates separate compile directories by default. This disables:
        CONFIG -= debug_and_release debug_and_release_target
        CONFIG += mingw
        DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX
        DEFINES -= UNICODE
        # win32-packager.pl builds Qt under DOS, but MythTV is built in MinGW.
        # This corrects the moc tool path from a DOS-style to a unix style:
        QMAKE_MOC = $$[QT_INSTALL_BINS]/moc
        QMAKE_EXTENSION_SHLIB = dll

        isEmpty(QMAKE_EXTENSION_LIB) {
            QMAKE_EXTENSION_LIB=a
        }
        MYTH_LIB_EXT  =$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}
    }

    # if CYGWIN compile, set up flag in CONFIG
    contains(TARGET_OS, CYGWIN) {

        CONFIG += cygwin
        QMAKE_EXTENSION_SHLIB=dll.a
    }

} else {

    # All Others

    isEmpty(QMAKE_EXTENSION_SHLIB) {
        QMAKE_EXTENSION_SHLIB=so
    }
    isEmpty(QMAKE_EXTENSION_LIB) {
        QMAKE_EXTENSION_LIB=a
    }

    # For dependencies on Myth library filenames in POST_TARGETDEPS
    MYTH_SHLIB_EXT=$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
    MYTH_LIB_EXT  =$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}

    INCLUDEPATH += $$unique(CONFIG_INCLUDEPATH)

    LOCAL_LIBDIR_OGL =
    !isEmpty( QMAKE_LIBDIR_OPENGL ) {
        LOCAL_LIBDIR_OGL = -L$$QMAKE_LIBDIR_OPENGL
    }
    QMAKE_LIBDIR_OPENGL =

    # construct linking path
    LOCAL_LIBDIR_X11 =
    !isEmpty( QMAKE_LIBDIR_X11 ) {
        LOCAL_LIBDIR_X11 = -L$$QMAKE_LIBDIR_X11
    }
    QMAKE_LIBDIR_X11 =

    EXTRA_LIBS  = $$EXTRALIBS

    EXTRA_LIBS += $$CONFIG_FIREWIRE_LIBS

    EXTRA_LIBS += $$LOCAL_LIBDIR_OGL
    EXTRA_LIBS += $$LOCAL_LIBDIR_X11
    # FIXME MK Jan/20 I'm not sure this is necessary or necessarily accurate.
    # FFmpeg OpenGL is an option that we do not (and should not imho) enable -
    # and we should in that case be stripping out GLES etc as well.
    # and CONFIG_OPENGL_LIBS is always empty as we never set gl_lib anymore
    !isEmpty( CONFIG_OPENGL_LIBS ) {
        # Replace FFmpeg's OpenGL with OpenGLES
        EXTRA_LIBS -= -lGL
        EXTRA_LIBS += $$CONFIG_OPENGL_LIBS
    }

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

    # Allow compilation with Qt Embedded, if Qt is compiled without "-fno-rtti"
    QMAKE_CXXFLAGS -= -fno-exceptions -fno-rtti

    MOC_DIR         = moc
    OBJECTS_DIR     = obj
}

# Globals in static libraries need special treatment on OS X
macx:QMAKE_CFLAGS_STATIC_LIB += -fno-common

# figure out compile flags based on qmake info
# qmake 4.8.2 & 4.8.3 messes up OSX "-arch i386 -arch x86_64"
# clang 3.0 on Linux does not like duplicate arguments.
macx {
    QMAKE_CFLAGS   += $$CPPFLAGS   $$CFLAGS
    QMAKE_CXXFLAGS += $$CXXPPFLAGS $$CXXFLAGS $$ECXXFLAGS
} else {
    QMAKE_CFLAGS   *= $$CPPFLAGS   $$CFLAGS
    QMAKE_CXXFLAGS *= $$CXXPPFLAGS $$CXXFLAGS $$ECXXFLAGS
}

profile:!win32:!macx:CONFIG += debug

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

macx {
    using_firewire:using_backend:EXTRA_LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
    QMAKE_LFLAGS_SONAME  = -Wl,-install_name,@rpath/
    _RPATH_="-rpath,"
} else {
    _RPATH_="-rpath="
}

using_compdb:contains(CC, clang) {
    QMAKE_CFLAGS += "-MJ $@.json"
    QMAKE_CXXFLAGS += "-MJ $@.json"
}
