win32-msvc* {

  SRC_PATH_BARE = $$(SRC_PATH_BARE)

  isEmpty( $$SRC_PATH_BARE ) {
    SRC_PATH_BARE = $${PWD}
  }

  CONFIG -= debug_and_release
  CONFIG -= debug_and_release_target
  CONFIG -= flat

  CONFIG *= using_backend using_frontend
  CONFIG *= using_opengl
  CONFIG *= using_hdhomerun
  CONFIG *= using_satip

  CONFIG_LIBMPEG2EXTERNAL = yes
  CONFIG_QTDBUS = no

  QMAKE_CXXFLAGS_WARN_ON -= -w34100

} else {

  include ( config.mak )
}

contains(QT_MAJOR_VERSION, 6) {
QT += core5compat
}
CONFIG += $$CCONFIG
CONFIG += c++17
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
  win32-msvc* {
    PREFIX = "."
  } else {
    PREFIX = /usr/local
  }
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

LIBVERSION = 32
VERSION = 32.0

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

    # All versions of Microsoft Visual Studio

    win32-msvc* {

        win32-msvc2010 {
            # need to force include missing math.h functions.

            # needed for vcxproj
            QMAKE_CXXFLAGS += "/FI mathex.h"

            # needed for nmake
            QMAKE_CFLAGS   += "/FI mathex.h"
        }

        DEFINES += _WIN32 WIN32 WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES
        DEFINES += _CRT_SECURE_NO_WARNINGS

        debug  :DEFINES += _DEBUG
        release:DEFINES += NDEBUG

        # msvc specific include path

        INCLUDEPATH += ./
        INCLUDEPATH += $$SRC_PATH_BARE/external

        INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/include
        INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external/pthreads.2
        INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external/zlib
        INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external

        win32-msvc2010:INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/include-2010

        INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external/exiv2/msvc64/include

        # have visual studio place all DLL, EXE & lib files in the following directory

        CONFIG( debug, debug|release) {

            # debug

            DESTDIR         = $$SRC_PATH_BARE/bin/debug
            QMAKE_LIBDIR   += $$SRC_PATH_BARE/bin/debug
            MOC_DIR         = debug/moc
            OBJECTS_DIR     = debug/obj

            QMAKE_CXXFLAGS *= /MDd /MP /wd4100 /wd4996

            LIBS           += -L$$SRC_PATH_BARE/bin/debug
            EXTRA_LIBS     += -lpthreadVC2d -L$$SRC_PATH_BARE/bin/debug

        } else {

            # release

            DESTDIR         = $$SRC_PATH_BARE/bin/release
            QMAKE_LIBDIR   += $$SRC_PATH_BARE/bin/release
            MOC_DIR         = release/moc
            OBJECTS_DIR     = release/obj

            QMAKE_CXXFLAGS *= /MD /MP /wd4100 /wd4996

            LIBS           += -L$$SRC_PATH_BARE/bin/release
            EXTRA_LIBS     += -lpthreadVC2 -L$$SRC_PATH_BARE/bin/release

        }

    }

    # minGW Build Environment

    mingw {

        # Qt4 creates separate compile directories by default. This disables:
        CONFIG -= debug_and_release debug_and_release_target
        CONFIG += mingw
        DEFINES += WIN32 USING_MINGW WIN32_LEAN_AND_MEAN NOMINMAX
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
        DEFINES += CONFIG_CYGWIN
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

release:contains( ARCH_POWERPC, yes ) {
    # Auto-inlining causes some Qt moc methods to go missing
    macx:QMAKE_CXXFLAGS_RELEASE += -fno-inline-functions
}

# figure out defines
DEFINES += $$CONFIG_DEFINES
DEFINES += _GNU_SOURCE

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
