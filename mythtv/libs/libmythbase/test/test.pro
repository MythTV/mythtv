include (../../../settings.pro)

TEMPLATE = subdirs

SUBDIRS += test_*

unittest.target = test
unittest.commands = ../../../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
