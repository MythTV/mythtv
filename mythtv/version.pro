############################################################
# Optional qmake instructions which automatically generate #
# an extra source file containing the Subversion revision. #
#       If the directory has no .svn directories,          #
#        "exported" is reported as the revision.           #
############################################################

SVNTREEDIR = $$system(pwd)
SVNREPOPATH = "$$URL$$"


SOURCES += version.cpp

version.target = version.cpp 

version.commands = sh \"$$SVNTREEDIR/version.sh\" \"$$SVNTREEDIR\" \"$$SVNREPOPATH\"

!mingw: version.depends = FORCE 

QMAKE_EXTRA_UNIX_TARGETS += version
mingw: QMAKE_EXTRA_WIN_TARGETS += version
