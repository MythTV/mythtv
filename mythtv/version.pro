############################################################
# Optional qmake instructions which automatically generate #
# an extra source file containing the Subversion revision. #
#       If the directory has no .svn directories,          #
#        "exported" is reported as the revision.           #
############################################################

SVNVERSION = $$system(svnversion . 2>/dev/null)

isEmpty( SVNVERSION ) {
    SVNVERSION = Unknown
}

SOURCES += version.cpp

version.target = version.cpp 
version.commands = echo 'const char *myth_source_version =' \
'"'`(svnversion . 2>/dev/null) || echo Unknown`'";' > version.cpp
version.depends = FORCE 

QMAKE_EXTRA_UNIX_TARGETS += version
