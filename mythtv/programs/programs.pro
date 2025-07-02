include ( ../settings.pro )

TEMPLATE = subdirs

# unit tests mythcommflag
mythcommflag-test.depends = sub-mythcommflag
mythcommflag-test.target = buildtestmythcommflag
mythcommflag-test.commands = cd mythcommflag/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += mythcommflag-test

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythjobqueue mythlcdserver
    SUBDIRS += mythshutdown mythutil
    !win32-*: SUBDIRS += mythwelcome
    SUBDIRS += mythpreviewgen mythmediaserver mythccextractor
    SUBDIRS += mythscreenwizard
    !mingw:!win32-msvc*: SUBDIRS += mythtranscode/external/replex

    # unit tests mythfrontend
    mythfrontend-test.depends = sub-mythfrontend
    mythfrontend-test.target = buildtestmythfrontend
    mythfrontend-test.commands = cd mythfrontend/test && $(QMAKE) && $(MAKE)
    unix:QMAKE_EXTRA_TARGETS += mythfrontend-test
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup 
    SUBDIRS += mythmetadatalookup

    !win32-msvc*:SUBDIRS += scripts
    !mingw:!win32-msvc*: SUBDIRS += mythfilerecorder
    !mingw:!win32-msvc*: SUBDIRS += mythexternrecorder

    # unit tests mythbackend
    mythbackend-test.depends = sub-mythbackend
    mythbackend-test.target = buildtestmythbackend
    mythbackend-test.commands = cd mythbackend/test && $(QMAKE) && $(MAKE)
    unix:QMAKE_EXTRA_TARGETS += mythbackend-test
}

using_mythtranscode: SUBDIRS += mythtranscode

unittest.depends = mythfrontend-test mythbackend-test mythcommflag-test
unittest.target = test
unittest.commands = scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
