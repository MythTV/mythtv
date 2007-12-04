############################################################
# Optional qmake instructions which automatically generate #
# an extra source file containing the Subversion revision. #
#       If the directory has no .svn directories,          #
#        "exported" is reported as the revision.           #
############################################################

# Get a string like "0.21.20071125-1"
MYTHBINVERS = $$system(egrep MYTH_BINARY_VERSION libs/libmyth/mythcontext.h | sed 's/.*MYTH_BINARY_VERSION //')

SVNTREEDIR = $$system(pwd)
SVNREPOPATH = "$$URL$$"


SOURCES += version.cpp

version.target = version.cpp 

version.commands = sh -c "echo 'const char *myth_source_version =' \
'\"'`(svnversion $${SVNTREEDIR} 2>/dev/null) || echo Unknown`'\";' \
> .vers.new"

version.commands += ; sh -c "echo 'const char *myth_source_path =' \
'\"'`echo $${SVNREPOPATH} \
| sed -e 's,.*/svn/,,' -e 's,/mythtv/version\.pro.*,,'`'\";' \
>> .vers.new"

version.commands += ; sh -c "echo 'const char *myth_binary_version =' \
'\"$$MYTHBINVERS\";' >> .vers.new"

version.commands += ; sh -c "diff .vers.new version.cpp > .vers.diff 2>&1 ; \
if test -s .vers.diff ; then mv -f .vers.new version.cpp ; fi ; \
rm -f .vers.new .vers.diff"

version.depends = FORCE 

QMAKE_EXTRA_UNIX_TARGETS += version
