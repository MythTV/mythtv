include (../../../settings.pro)

TEMPLATE = subdirs

SUBDIRS += $$files(test_*)

unittest.target = test
unittest.commands = ../../../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
