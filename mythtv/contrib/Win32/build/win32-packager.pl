#!c:/perl/bin/perl.exe -w
##############################################################################
### =file
### win32-packager.pl
###
### =location
### http://svn.mythtv.org/svn/trunk/mythtv/contrib/Win32/build/win32-packager.pl
###
### =description
### Tool for automating frontend builds on MS Windows XP (and compatible)
### originally based loosely on osx-packager.pl, but now is its own beast.
###
### =revision
### $Id$
###
### =author
### David Bussenschutt
##############################################################################

use strict;
use LWP::UserAgent;
use IO::File;
use Data::Dumper; 
use File::Copy qw(cp);

my $NOISY = 1; # set to 0 for less output to the screen

$| = 1; # autoflush stdout;

# this script was last tested to work with this version, on other versions YMMV.
my $SVNRELEASE = '16167'; # latest build that has been confirmed to run , but  seems to need 4 patches:  
#4724 http://svn.mythtv.org/trac/raw-attachment/ticket/4724/win32_storeconnection.patch 
#4706 http://svn.mythtv.org/trac/raw-attachment/ticket/4706/mythsocket_win32_events.patch 
#4699 http://svn.mythtv.org/trac/raw-attachment/ticket/4699/win32_fs.patch 
#4702 http://svn.mythtv.org/trac/raw-attachment/ticket/4702/mingw.patch 

#my $SVNRELEASE = 'HEAD' ;# if you are game, go forth and test the latest release! 

# We allow SourceForge to tell us which server to download from,
# rather than assuming specific server/s
my $sourceforge = 'downloads.sourceforge.net';     # auto-redirect to a
                                                   # mirror of SF's choosing,
                                                   # hopefully close to you
# alternatively you can choose your own mirror:
#my $sourceforge = 'optusnet.dl.sourceforge.net';  # Australia 
#my $sourceforge = 'internap.dl.sourceforge.net';  # USA,California
#my $sourceforge = 'easynews.dl.sourceforge.net';  # USA,Arizona,Phoenix,
#my $sourceforge = 'jaist.dl.sourceforge.net';     # Japan
#my $sourceforge = 'mesh.dl.sourceforge.net';      # Germany

# Set this to the empty string for no-proxy:
my $proxy = '';
#my $proxy = 'http://enter.your.proxy.here:8080';
# Subversion proxy settings are configured in %APPDATA%\Subversion\servers

# Main mythtv version - used to name dlls
my $version = 0.21;

# TODO -  use this list to define the components to build - only the first of these currently works well.
my @components = ( 'mythtv', 'myththemes', 'mythplugins' );


# TODO - we should try to autodetect these paths, rather than assuming
#        the defaults - perhaps from environment variables like this:
#  die "must have env variable SOURCES pointing to your sources folder"
#      unless $ENV{SOURCES};
#  my $sources = $ENV{SOURCES};
# TODO - although theoretically possible to change these paths,
#        it has NOT been tested much, and will with HIGH PROBABILITY fail somewhere. 
# TODO - Only $mingw is tested and most likely is safe to change.

# perl compatible paths (single forward slashes in DOS style):
my $msys = 'C:/MSys/1.0/'; # must end in slash, and use forward slashes /
my $sources = 'C:/msys/1.0/sources/'; # must end in slash, and use forward slashes /
my $mingw = 'C:/MinGW/'; # must end in slash, and use forward slashes /
my $mythtv = 'C:/mythtv/';  # this is where the entire SVN checkout lives so c:/mythtv/mythtv/ is the main codebase.  # must end in slash, and use forward slashes /

# DOS executable CMD.exe versions of the paths (for when we shell to DOS mode):
my $dosmsys    = perl2dos($msys);
my $dossources = perl2dos($sources);
my $dosmingw   = perl2dos($mingw); 
my $dosmythtv  = perl2dos($mythtv);

# unix/msys equivalent versions of the paths (for when we shell to MSYS/UNIX mode):
my $unixmsys = '/'; # msys root is always mounted here, irrespective of where DOS says it really is.
my $unixmingw = '/mingw/'; # mingw is always mounted here under unix, if you setup mingw right in msys, so we will usually just say /mingw in the code, not '.$unixmingw.' or similar (see /etc/fstab)
my $unixsources = perl2unix($sources); $unixsources =~ s#$unixmsys#/#i;  #strip leading msys path, if there, it's unnecessary as it's mounted under /
my $unixmythtv  = perl2unix($mythtv);


#NOTE: IT'S IMPORTANT that the PATHS use the correct SLASH-ing method for
#the type of action:
#      for [exec] actions, use standard DOS paths, with single BACK-SLASHES
#      '\' (unless in double quotes, then double the backslashes)
#      for [shell] actions, use standard UNIX paths, with single
#      FORWARD-SLASHES '/'
#
#NOTE: when referring to variables in paths, try to keep them out of double
#quotes, or the slashing can get confused:
#      [exec]   actions should always refer to  $dosXXX path variables
#      [shell]  actions should always refer to $unixXXX path  variables
#      [dir],[file],[mkdirs],[archive] actions should always refer to
#      default perl compatible paths
# NOTE:  The architecture of this script is based on cause-and-event.  
#        There are a number of "causes" (or expectations) that can trigger
#        an event/action.
#        There are a number of different actions that can be taken.
#
# eg: [ dir  => "c:/MinGW", exec => $dossources.'MinGW-5.1.3.exe' ],
#
# means: expect there to be a dir called "c:/MinGW", and if there isn't
# execute the file MinGW-5.1.3.exe. (clearly there needs to be a file
# MinGW-5.1.3.exe on disk for that to work, so there is an earlier
# declaration to 'fetch' it)

# Script later creates %HOMEPATH%\.mythtv\mysql.txt
if ( ! exists $ENV{'HOMEPATH'} || $ENV{'HOMEPATH'} eq '\\' )
{   $ENV{'HOMEPATH'} = $ENV{'USERPROFILE'}   }

#build expectations (causes) :
#  missing a file (given an expected path)                                 [file]
#  missing folder                                                          [dir]
#  missing source archive (fancy version of 'file' to fetch from the web)  [archive]
#  apply a perl pattern match and if it DOESNT match execute the action    [grep]  - this 'cause' actually needs two parameters in an array [ pattern, file].  If the file is absent, the pattern is assumed to not match (and emits a warning).
#  test the file/s are totally the same (by size and mtime)                [filesame] - if first file is non-existant then that's permitted, it causes the action to trigger.
#  test the first file is newer(mtime) than the second one                 [newer] - if given 2 existing files, not necessarily same size/content, and the first one isn't newer, execute the action!.  If the first file is ABSENT, run the action too.

#build actions (events) are:
#  fetch a file from the web (to a location)                         [fetch]
#  set an environment variable (name, value pair)                    not-yet-impl
#  execute a DOS/Win32 exe/command and wait to complete              [exec]
#  execute a MSYS/Unix script/command in bash and wait to complete   [shell]- this 'effect' actually accepts many parameters in an array [ cmd1, cmd2, etc ]
#  extract a .tar .tar.gz or .tar.bz2 or ,zip file ( to a location)  [extract] - (note that .gz and .bz2 are thought equivalent)
#  write a small patch/config/script file directly to disk           [write]
#  make directory tree upto the path specified                       [mkdirs]
#  copy a new version of a file, set mtime to the original           [copy] 

#TODO:
#  copy a set of files (path/filespec,  destination)                 not-yet-impl.  use exec => 'copy /Y xxx.* yyy'
#  apply a diff                                                      not-yet-impl   use shell => 'patch -p0 < blah.patch'
#  search-replace text in a file                                     not-yet-impl   use grep => ['pattern',subject], exec => shell 'patch < etc to replace it'


# NOTES on specific actions: 
# 'extract' now requires all paths to be perl compatible (like all other
# commands) If not supplied, it extracts into the folder the .tar.gz is in.
# 'exec' actually runs all your commands inside a single cmd.exe
# command-line. To link commands use '&&'
# 'shell' actually runs all your commands inside a bash shell with -c "(
# cmd;cmd;cmd )" so be careful about quoting.


#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
# DEFINE OUR EXPECTATIONS and the related ACTIONS:
#  - THIS IS THE GUTS OF THE APPLICATION!
#  - A SET OF DECLARATIONS THAT SHOULD RESULT IN A WORKING WIN32 INSTALATION
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------

my $expect;

push @{$expect},

[ dir => [$sources] , mkdirs => [$sources], comment => 'We download all the files from the web, and save them here:'],


[ archive => $sources.'MinGW-5.1.3.exe', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/MinGW-5.1.3.exe',comment => 'Get mingw and addons first, or we cant do [shell] requests!' ],
#[ archive => $sources.'mingw32-make-3.81-2.tar', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/mingw32-make-3.81-2.tar.gz' ], - not needed if you select it during the MinGW install
[ archive => $sources.'mingw-utils-0.3.tar.gz', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/mingw-utils-0.3.tar.gz' ],


[ dir  => $mingw, exec => $dossources.'MinGW-5.1.3.exe',comment => 'install MinGW (be sure to install g++, g77 and ming make too) - it will require user interaction, but once completed, will return control to us....' ], # interactive, supposed to install g++ and ming make too, but people forget to select them? 
[ file  => $mingw."bin/gcc.exe", exec => $dossources.'MinGW-5.1.3.exe',comment => 'unable to find a gcc.exe where expected, rerunning MinGW installer!' ], # interactive, supposed to install g++ and ming make too, but people forget to select them? 

[ archive => $sources.'MSYS-1.0.10.exe', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/MSYS-1.0.10.exe',comment => 'Get the MSYS and addons:' ] ,
[ archive => $sources.'bash-3.1-MSYS-1.0.11-1.tar.bz2', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/bash-3.1-MSYS-1.0.11-1.tar.bz2' ] ,
[ archive => $sources.'zlib-1.2.3-MSYS-1.0.11.tar.bz2', 'fetch' => 'http://easynews.dl.sourceforge.net/sourceforge/mingw/zlib-1.2.3-MSYS-1.0.11.tar.bz2' ] ,
[ archive => $sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2' ] ,

# install MSYS, it supplies the 'tar' executable, among others:
[ file => $msys.'bin/tar.exe', exec => $dossources.'MSYS-1.0.10.exe',comment => 'Install MSYS, it supplies the tar executable, among others. You should follow prompts, AND do post-install in DOS box.' ] , 

#  don't use the [shell] or [copy] actions here, as neither are available until bash is installed!
[ file => $msys.'bin/sh2.exe', exec => 'copy /Y '.$dosmsys.'bin\sh.exe '.$dosmsys.'bin\sh2.exe',comment => 'make a copy of the sh.exe so that we can utilise it when we extract later stuff' ],

# prior to this point you can't use the 'extract' 'copy' or 'shell' features!

# if you did a default-install of MingW, then you need to try again, as we
# really need g++ and mingw32-make, and g77 is needed for fftw
[ file => $mingw.'bin/mingw32-make.exe',  exec => $dossources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],
[ file => $mingw.'bin/g++.exe', exec => $dossources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],
[ file => $mingw.'bin/g77.exe', exec => $dossources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],

#[ file => 'C:/MinGW/bin/mingw32-make.exe',  extract =>
#$sources.'mingw32-make-3.81-2.tar',"C:/MinGW" ], - optionally we could get
#mingw32-make from here

# now that we have the 'extract' feature, we can finish ...
[ file => $mingw.'/bin/reimp.exe',  extract => [$sources.'mingw-utils-0.3.tar', $mingw],comment => 'Now we can finish all the mingw and msys addons:' ],
[ file => $msys.'bin/bash.exe',  extract => [$sources.'bash-3.1-MSYS-1.0.11-1.tar', $msys] ],
[ dir => $sources.'coreutils-5.97',  extract => [$sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar'] ],
[ file => $msys.'bin/pr.exe', shell => ["cd ".$unixsources."coreutils-5.97","cp -r * / "] ],

[ dir => $msys."lib" ,  mkdirs => $msys.'lib' ],
[ dir => $msys."include" ,  mkdirs => $msys.'include' ],

#get gdb
[ archive => $sources.'gdb-6.7.50.20071127-mingw.tar.bz2', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/gdb-6.7.50.20071127-mingw.tar.bz2',comment => 'Get gdb for possible debugging later' ],
[ file => $msys.'bin/gdb.exe',  extract => [$sources.'gdb-6.7.50.20071127-mingw.tar.bz2', $msys] ],


# (alternate would be from the gnuwin32 project, which is actually from same source)
#  run it into a 'unzip' folder, because it doesn't extract to a folder:
[ dir => $sources."unzip" ,  mkdirs => $sources.'unzip',comment => 'unzip.exe - Get a precompiled native Win32 version from InfoZip' ],
[ archive => $sources.'unzip/unz552xN.exe',  fetch => 'ftp://tug.ctan.org/tex-archive/tools/zip/info-zip/WIN32/unz552xN.exe'],
[ file => $sources.'unzip/unzip.exe', exec => 'chdir '.$dossources.'unzip && '.$dossources.'unzip/unz552xN.exe' ],
# we could probably put the unzip.exe into the path...


[ archive => $sources.'svn-win32-1.4.6.zip',  fetch => 'http://subversion.tigris.org/files/documents/15/41077/svn-win32-1.4.6.zip',comment => 'Subversion comes as a zip file, so it cant be done earlier than the unzip tool!'],
[ dir => $sources.'svn-win32-1.4.6', extract => $sources.'svn-win32-1.4.6.zip' ],


[ file => $msys.'bin/svn.exe', shell => ["cp -R $unixsources/svn-win32-1.4.6/* ".$unixmsys],comment => 'put the svn.exe executable into the path, so we can use it easily later!' ],

# :
[ dir => $sources."zlib" ,  mkdirs => $sources.'zlib',comment => 'the zlib download is a bit messed-up, and needs some TLC to put everything in the right place' ],
[ dir => $sources."zlib/usr",  extract => [$sources.'zlib-1.2.3-MSYS-1.0.11.tar', $sources."zlib"] ],
# install to /usr:
[ file => $msys.'lib/libz.a',      exec => ["copy /Y ".$dossources.'zlib\usr\lib\* '.$dosmsys."lib"] ], 
[ file => $msys.'bin/msys-z.dll',  exec => ["copy /Y ".$dossources.'zlib\usr\bin\* '.$dosmsys."bin"] ],
[ file => $msys.'include/zlib.h',  exec => ["copy /Y ".$dossources.'zlib\usr\include\* '.$dosmsys."include"] ],
# taglib also requires zlib in /mingw , so we'll put it there too, why not! 
[ file => $mingw.'lib/libz.a',      exec => ["copy /Y ".$dossources.'zlib\usr\lib\* '.$dosmingw."lib"] ],
[ file => $mingw.'bin/msys-z.dll',  exec => ["copy /Y ".$dossources.'zlib\usr\bin\* '. $dosmingw."bin"] ],
[ file => $mingw.'include/zlib.h',  exec => ["copy /Y ".$dossources.'zlib\usr\include\* '.$dosmingw."include"] ],

# fetch mysql
# primary server site is:
# http://dev.mysql.com/get/Downloads/MySQL-5.0/mysql-essential-5.0.45-win32.msi/from/http://mysql.mirrors.ilisys.com.au/
[ archive => $sources.'mysql-essential-5.0.45-win32.msi', 'fetch' => 'http://mirror.services.wisc.edu/mysql/Downloads/MySQL-5.0/mysql-essential-5.0.45-win32.msi',comment => 'fetch mysql binaries - this is a big download(23MB) so it might take a while' ],
[ file => "c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll", exec => $dossources.'mysql-essential-5.0.45-win32.msi',comment => 'Install mysql - be sure to choose to do a "COMPLETE" install. You should also choose NOT to "configure the server now" ' ],

# after mysql install 
[ filesame => [$mingw.'bin/libmySQL.dll','c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll'],  copy => [''=>'',comment => 'post-mysql-install'] ],
[ filesame => [$mingw.'lib/libmySQL.dll','c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll'],  copy => [''=>'',comment => 'post-mysql-install'] ],
[ filesame => [$mingw.'lib/libmysql.lib','c:/Program Files/MySQL/MySQL Server 5.0/lib/opt/libmysql.lib'],  copy => [''=>''] ],
[ file => $mingw.'include/mysql.h'  ,   exec => 'copy /Y "c:\Program Files\MySQL\MySQL Server 5.0\include\*" '.$dosmingw."include" ],
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/include/* /c/MinGW/include/
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/bin/libmySQL.dll /c/MinGW/lib
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/lib/opt/libmysql.lib /c/MinGW/lib

#
# TIP: we use a special file (with an extra _ ) as a marker to do this
# action every the time, it's harmless to do it more often that required.
# 'nocheck' means continue even if the cause doesn't exist after.
[ file => $mingw.'lib/libmysql.lib__',  shell => ["cd /mingw/lib","reimp -d libmysql.lib","dlltool -k --input-def libmysql.def --dllname libmysql.dll --output-lib libmysql.a",'nocheck'],comment => ' rebuild libmysql.a' ],

# grep => [pattern,file] , actions/etc
[ file => $mingw.'include/mysql___h.patch', write => [$mingw.'include/mysql___h.patch',
'--- mysql.h_orig	Fri Jan  4 19:35:33 2008
+++ mysql.h	Tue Jan  8 14:48:36 2008
@@ -41,11 +41,9 @@

 #ifndef _global_h				/* If not standard header */
 #include <sys/types.h>
-#ifdef __LCC__
 #include <winsock.h>				/* For windows */
-#endif
 typedef char my_bool;
-#if (defined(_WIN32) || defined(_WIN64)) && !defined(__WIN__)
+#if (defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__)) && !defined(__WIN__)
 #define __WIN__
 #endif
 #if !defined(__WIN__)
 
 ' ],comment => 'write the patch for the the mysql.h file'],


[ grep => ['\|\| defined\(__MINGW32__\)',$mingw.'include/mysql.h'], shell => ["cd /mingw/include","patch -p0 < mysql___h.patch"],comment => 'Apply mysql.h patch file, if not already applied....' ],


# fetch it
[ dir =>     $sources.'pthread', mkdirs => $sources.'pthread' ],
[ archive => $sources.'pthread/libpthread.a',   'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/lib/libpthreadGC2.a',comment => 'libpthread is precompiled, we just download it to the right place ' ],
[ archive => $sources.'pthread/pthreadGC2.dll', 'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/lib/pthreadGC2.dll' ],
[ filesame    => [$sources.'pthread/pthread.dll',$sources."pthread/pthreadGC2.dll"],  copy => [''=>''] ],
[ archive => $sources.'pthread/pthread.h',      'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/pthread.h' ],
[ archive => $sources.'pthread/sched.h',        'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/sched.h' ],
[ archive => $sources.'pthread/semaphore.h',    'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/semaphore.h' ],
# install it:
[ filesame => [$mingw.'lib/libpthread.a',   $sources."pthread/libpthread.a"],      copy => [''=>'',comment => 'install pthread'] ],
[ filesame => [$mingw.'bin/pthreadGC2.dll', $sources."pthread/pthreadGC2.dll"],    copy => [''=>''] ],
[ filesame => [$mingw.'bin/pthread.dll',    $sources."pthread/pthread.dll"],       copy => [''=>''] ],
[ filesame => [$mingw.'include/pthread.h',  $sources."pthread/pthread.h"],         copy => [''=>''] ],
[ filesame => [$mingw.'include/sched.h',    $sources."pthread/sched.h"],           copy => [''=>''] ],
[ filesame => [$mingw.'include/semaphore.h',$sources."pthread/semaphore.h"],       copy => [''=>''] ],


#   ( save bandwidth compare to the above full SDK where they came from:
[ archive => $sources.'DX9SDK_dsound_Include_subset.zip', 'fetch' => 'http://davidbuzz.googlepages.com/DX9SDK_dsound_Include_subset.zip',comment => 'We download just the required Include files for DX9' ], 
[ dir => $sources.'DX9SDK_dsound_Include_subset', extract => $sources.'DX9SDK_dsound_Include_subset.zip' ],
[ filesame => [$mingw.'include/dsound.h',$sources."DX9SDK_dsound_Include_subset/dsound.h"], copy => [''=>''] ],
[ filesame => [$mingw.'include/dinput.h',$sources."DX9SDK_dsound_Include_subset/dinput.h"], copy => [''=>''] ],
[ filesame => [$mingw.'include/ddraw.h', $sources."DX9SDK_dsound_Include_subset/ddraw.h"],  copy => [''=>''] ],
[ filesame => [$mingw.'include/dsetup.h',$sources."DX9SDK_dsound_Include_subset/dsetup.h"], copy => [''=>''] ],


#----------------------------------------
# now we do each of the source library dependencies in turn:
# download,extract,build/install
# TODO - ( and just pray that they all work?)  These should really be more
# detailed, and actually check that we got it installed properly.
# Most of these look for a Makefile as a sign that the ./configure was
# successful (not necessarily true, but it's a start) but this requires that
# the .tar.gz didn't come with a Makefile in it.

[ archive => $sources.'freetype-2.3.5.tar.gz',  fetch => 'http://download.savannah.nongnu.org/releases/freetype/freetype-2.3.5.tar.gz'],
[ dir => $sources.'freetype-2.3.5', extract => $sources.'freetype-2.3.5.tar' ],
# caution... freetype comes with a Makefile in the .tar.gz, so work around it!
[ file => $sources.'freetype-2.3.5/Makefile_', shell => ["cd $unixsources/freetype-2.3.5","./configure --prefix=/mingw","touch $unixsources/freetype-2.3.5/Makefile_"] ],
# here's an example of specifying the make and make install steps separately, for apps that can't be relied on to have the make step work!
[ file => $sources.'freetype-2.3.5/objs/.libs/libfreetype.a', shell => ["cd $unixsources/freetype-2.3.5","make"] ],
[ file => $mingw.'lib/libfreetype.a', shell => ["cd $unixsources/freetype-2.3.5","make install"] ],


[ archive => $sources.'lame-3.97.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/lame/lame-3.97.tar.gz'],
[ dir => $sources.'lame-3.97', extract => $sources.'lame-3.97.tar' ],
[ file => $sources.'lame-3.97/Makefile', shell => ["cd $unixsources/lame-3.97","./configure --prefix=/mingw","make","make install"] ],

[ archive => $sources.'libmad-0.15.1b.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/mad/libmad-0.15.1b.tar.gz'],
[ dir => $sources.'libmad-0.15.1b', extract => $sources.'libmad-0.15.1b.tar' ],
[ file => $sources.'libmad-0.15.1b/Makefile', shell => ["cd $unixsources/libmad-0.15.1b","./configure --prefix=/usr","make","make install"] ],


[ archive => $sources.'taglib-1.4.tar.gz',  fetch => 'http://developer.kde.org/~wheeler/files/src/taglib-1.4.tar.gz'],
[ dir => $sources.'taglib-1.4', extract => $sources.'taglib-1.4.tar' ],
[ file => $sources.'taglib-1.4/Makefile', shell => ["cd $unixsources/taglib-1.4","./configure --prefix=/mingw","make","make install"] ],
# TODO tweak makefiles:
# INSTALL = C:/msys/1.0/bin/install -c -p
# INSTALL = ../C:/msys/1.0/bin/install -c -p
# INSTALL = ../../C:/msys/1.0/bin/install -c -p


[ archive => $sources.'libao-0.8.8.tar.gz',  fetch => 'http://downloads.xiph.org/releases/ao/libao-0.8.8.tar.gz'],
[ dir => $sources.'libao-0.8.8', extract => $sources.'libao-0.8.8.tar' ],
[ file => $sources.'libao-0.8.8/Makefile', shell => ["cd $unixsources/libao-0.8.8","./configure --prefix=/usr","make","make install"] ],

[ archive => $sources.'libogg-1.1.3.tar.gz',  fetch => 'http://downloads.xiph.org/releases/ogg/libogg-1.1.3.tar.gz'],
[ dir => $sources.'libogg-1.1.3', extract => $sources.'libogg-1.1.3.tar' ],
[ file => $sources.'libogg-1.1.3/Makefile', shell => ["cd $unixsources/libogg-1.1.3","./configure --prefix=/usr","make","make install"] ],

[ archive => $sources.'libvorbis-1.2.0.tar.gz',  fetch => 'http://downloads.xiph.org/releases/vorbis/libvorbis-1.2.0.tar.gz'],
[ dir => $sources.'libvorbis-1.2.0', extract => $sources.'libvorbis-1.2.0.tar' ],
[ file => $sources.'libvorbis-1.2.0/Makefile', shell => ["cd $unixsources/libvorbis-1.2.0","./configure --prefix=/usr --disable-shared","make","make install"] ],


[ archive => $sources.'flac-1.2.1.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/flac/flac-1.2.1.tar.gz'],
[ dir => $sources.'flac-1.2.1', extract => $sources.'flac-1.2.1.tar' ],
[ grep => ['\#define SIZE_T_MAX UINT_MAX',$mingw.'include/limits.h'], shell => "echo \\#define SIZE_T_MAX UINT_MAX >> $mingw/include/limits.h" ], 
[ file => $sources.'flac-1.2.1/Makefile', shell => ["cd $unixsources/flac-1.2.1","./configure --prefix=/mingw","make","make install"] ],

# skip doing pthreads from source, we use binaries that were fetched earlier:
#[ archive => $sources.'pthreads-w32-2-8-0-release.tar.gz',  fetch => 'ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-8-0-release.tar.gz'],
#[ dir => $sources.'pthreads-w32-2-8-0-release', extract => $sources.'pthreads-w32-2-8-0-release.tar' ],

[ archive => $sources.'SDL-1.2.12.tar.gz',  fetch => 'http://www.libsdl.org/release/SDL-1.2.12.tar.gz'],
[ dir => $sources.'SDL-1.2.12', extract => $sources.'SDL-1.2.12.tar.gz' ],
[ file => $sources.'SDL-1.2.12/Makefile', shell => ["cd $unixsources/SDL-1.2.12","./configure --prefix=/usr","make","make install"] ],


[ archive => $sources.'tiff-3.8.2.tar.gz',  fetch => 'ftp://ftp.remotesensing.org/pub/libtiff/tiff-3.8.2.tar.gz'],
[ dir => $sources.'tiff-3.8.2', extract => $sources.'tiff-3.8.2.tar' ],
[ file => $sources.'tiff-3.8.2/Makefile', shell => ["cd $unixsources/tiff-3.8.2","./configure --prefix=/usr","make","make install"] ],


[ archive => $sources.'libexif-0.6.16.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/libexif/libexif-0.6.16.tar.gz'],
[ dir => $sources.'libexif-0.6.16', extract => $sources.'libexif-0.6.16.tar' ],
[ file => $sources.'libexif-0.6.16/Makefile', shell => ["cd $unixsources/libexif-0.6.16","./configure --prefix=/usr","make","make install"] ],


[ archive => $sources.'libvisual-0.4.0.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/libvisual/libvisual-0.4.0.tar.gz'],
[ dir => $sources.'libvisual-0.4.0', extract => $sources.'libvisual-0.4.0.tar' ],
[ file => $sources.'libvisual-0.4.0/Makefile', shell => ["cd $unixsources/libvisual-0.4.0","./configure --prefix=/usr","make","make install"] ],


[ archive => $sources.'fftw-3.1.2.tar.gz',  fetch => 'http://www.fftw.org/fftw-3.1.2.tar.gz'],
[ dir => $sources.'fftw-3.1.2', extract => $sources.'fftw-3.1.2.tar' ],
[ file => $sources.'fftw-3.1.2/Makefile', shell => ["cd $unixsources/fftw-3.1.2","./configure --prefix=/mingw","make","make install"] ],


# typical template:
#[ archive => $sources.'xxx.tar.gz',  fetch => ''],
#[ dir => $sources.'xxx', extract => $sources.'xxx.tar' ],
#[ file => $sources.'xxx/Makefile', shell => ["cd $unixsources/xxx","./configure --prefix=/usr","make","make install"] ],

#----------------------------------------
# Building QT is complicated!

[ archive => $sources.'qt-3.3.x-p8.tar.bz2',  fetch => 'http://'.$sourceforge.'/sourceforge/qtwin/qt-3.3.x-p8.tar.bz2'],
[ dir => $msys.'qt-3.3.x-p8', extract => [$sources.'qt-3.3.x-p8.tar', $msys] ],

# two older patches
#[ archive => 'qt.patch.gz' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4270/qt.patch.gz'],
#[ archive => 'qt2.patch' , 'fetch' => 'http://tanas.ca/qt2.patch'],
# OR:
# equivalent patch:
[ archive => $sources.'qt.patch' , 'fetch' => 'http://tanas.ca/qt.patch',comment => ' patch the QT sources'],
[ filesame => [$msys.'qt-3.3.x-p8/qt.patch',$sources."qt.patch"], copy => [''=>''] ],
[ grep => ["\-lws2_32", $msys.'qt-3.3.x-p8/mkspecs/win32-g++/qmake.conf'], shell => ["cd ".$unixmsys."qt-3.3.x-p8/","patch -p1 < qt.patch"] ],


[ file => $msys.'bin/sh_.exe', shell => ["mv ".$unixmsys."bin/sh.exe ".$unixmsys."bin/sh_.exe"],comment => 'rename msys sh.exe out of the way before building QT! ' ] ,

# write a batch script for the QT environment under DOS:
[ file => $msys.'qt-3.3.x-p8/qt_env.bat', write => [$msys.'qt-3.3.x-p8/qt_env.bat',
'rem a batch script for the QT environment under DOS:
set QTDIR='.$dosmsys.'qt-3.3.x-p8
set MINGW='.$dosmingw.'
set PATH=%QTDIR%\bin;%MINGW%\bin;%PATH%
set QMAKESPEC=win32-g++
cd %QTDIR%
'
],comment=>'write a batch script for the QT environment under DOS'],


[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll', exec => $dosmsys.'qt-3.3.x-p8\qt_env.bat && configure.bat -thread -plugin-sql-mysql -opengl -no-sql-sqlite',comment => 'Execute qt_env.bat  && the configure command to actually build QT now!  - ie configures qt and also makes it, hopefully! WARNING SLOW (MAY TAKE HOURS!)' ],

[ filesame => [$msys.'qt-3.3.x-p8/bin/libqt-mt3.dll',$msys.'qt-3.3.x-p8/lib/libqt-mt3.dll'], copy => [''=>''], comment => 'is there a libqt-mt3.dll in the "lib" folder of QT? if so, copy it to the the "bin" folder for uic.exe to use!' ],

# did the configure finish?  - run mingw32-make to get it to finish
# properly.
# HINT: the very last file built in a successful QT build env is the C:\msys\1.0\qt-3.3.x-p8\examples\xml\tagreader-with-features\tagreader-with-features.exe
[ file => $msys.'qt-3.3.x-p8/examples/xml/tagreader-with-features/tagreader-with-features.exe', exec => $dosmsys.'qt-3.3.x-p8\qt_env.bat && mingw32-make',comment => 'we try to finish the build of QT with mingw32-make, incase it was just a build dependancy issue? WARNING SLOW (MAY TAKE HOURS!)' ],

# TODO - do we have an action we can take to build just this one file/dll if it fails?  
# For now, we will just test if it built, and abort if it didn't!
[ file => $msys.'qt-3.3.x-p8/plugins/sqldrivers/libqsqlmysql.dll', exec => '', comment => 'lib\libqsqlmysql.dll - here we are just validating some basics of the the QT install, and if any of these components are missing, the build must have failed ( is the sql driver built properly?) '],
[ file => $msys.'qt-3.3.x-p8/bin/qmake.exe', exec => '', comment => 'bin\qmake.exe - here we are just validating some basics of the the QT install, and if any of these components are missing, the build must have failed'],
[ file => $msys.'qt-3.3.x-p8/bin/moc.exe', exec => '', comment => 'bin\moc.exe - here we are just validating some basics of the the QT install, and if any of these components are missing, the build must have failed'],
[ file => $msys.'qt-3.3.x-p8/bin/uic.exe', exec => '', comment => 'bin\uic.exe - here we are just validating some basics of the the QT install, and if any of these components are missing, the build must have failed'],


# TODO "make install" qt - where to?, will this work? (not sure, it
# sometimes might, but it's not critical?)
#[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll', exec => $dosmsys.'qt-3.3.x-p8\qt_env.bat && mingw32-make install',comment => 'install QT' ],
# a manual method for "installing" QT would be to put all the 'bin' files
# into /mingw/bin and similarly for the 'lib' and 'include' folders to their
# respective mingw folders?:


#  (back to sh.exe ) now that we are done !
[ file => $msys.'bin/sh.exe', shell => ["mv ".$unixmsys."bin/sh_.exe ".$unixmsys."bin/sh.exe"],comment => 'rename msys sh_.exe back again!' ] ,

#Copy libqt-mt3.dll to libqt-mt.dll  , if there is any date/size change!
[ filesame => [$msys.'qt-3.3.x-p8/lib/libqt-mt.dll',$msys.'qt-3.3.x-p8/lib/libqt-mt3.dll'], copy => [''=>''],comment => 'Copy libqt-mt3.dll to libqt-mt.dll' ] ,


#----------------------------------------

# get mythtv sources, if we don't already have them
# download all the files from the web, and save them here:
[ dir => $mythtv.'mythtv', mkdirs => $mythtv.'mythtv' ],


[ file => $mythtv.'svn_', shell => ["rm ".$unixmythtv."using_proxy_cannot_do_SVN.txt; if [ -n '$proxy' ] ; then touch ".$unixmythtv."using_proxy_cannot_do_SVN.txt; fi",'nocheck'],comment => 'disable SVN code fetching if we are using a proxy....' ],

# if we dont have the sources at all, get them all from SVN!  (do a
# checkout, but only if we don't already have the .pro file as a sign of an
# earlier checkout)
# this is some nasty in-line batch-script code, but it works.
;
# mythtv,mythplugins,myththemes
foreach my $comp( @components ) {
       push @{$expect}, [ dir => $mythtv.$comp, mkdirs => $mythtv.$comp ];
push @{$expect}, 
[ file => $mythtv.'using_proxy_cannot_do_SVN.txt', exec => ['set PATH='.$dosmsys.'bin;%PATH% && cd '.$dosmythtv.' && IF NOT EXIST '.$dosmythtv.'mythtv\mythtv.pro svn checkout  http://svn.mythtv.org/svn/trunk/'."$comp $comp",'nocheck'],comment => 'Get all the mythtv sources from SVN!:'.$comp ];
}
push @{$expect}, 
# now lets write some build scripts to help with mythtv itself

# 
[ file => $mythtv.'qt_env.sh', write => [$mythtv.'qt_env.sh',
'export QTDIR='.$unixmsys.'qt-3.3.x-p8
export QMAKESPEC=$QTDIR/mkspecs/win32-g++
export LD_LIBRARY_PATH=$QTDIR/lib:/usr/lib:/mingw/lib:/lib
export PATH=$QTDIR/bin:$PATH
' ],comment => 'write a script that we can source later when inside msys to setup the environment variables'],


# chmod the shell scripts, everytime
[ file => $mythtv.'_' , shell => ["cd $mythtv","chmod 755 *.sh",'nocheck'] ],

#----------------------------------------
# now we build mythtv! 
;

# SVN update every time, before patches, unless we are using a proxy
foreach my $comp( @components ) {
push @{$expect}, 
[ file => $mythtv.'using_proxy_cannot_do_SVN.txt', exec => ['cd '.$dosmythtv."$comp && ".$dosmsys.'bin\svn.exe -r '.$SVNRELEASE.' update','nocheck'],comment => 'getting SVN updates for:'.$comp ],
}
push @{$expect}, 

# always get svn num 
[ file => $mythtv.'_', exec => ['cd '.$dosmythtv.'mythtv && '.$dosmsys.'bin\svn.exe info > '.$dosmythtv.'mythtv\svn_info.new','nocheck'], comment => 'fetching the SVN number to a text file, if we can'],
[ filesame => [$mythtv.'mythtv\svn_info.txt',$mythtv.'mythtv\svn_info.new'], shell => ['touch -r '.$unixmythtv.'mythtv/svn_info.txt '.$unixmythtv.'mythtv/svn_info.new'], comment => 'match the datetime of these files, so that the contents only can be compared next' ],

# is svn num (ie file contents) changed since last run, if so, do a 'make
# clean' (overkill, I know, but safer)!
[ filesame => [$mythtv.'mythtv\svn_info.txt',$mythtv.'mythtv\svn_info.new'], shell => ['rm -f '.$unixmythtv.'delete_to_do_make_clean.txt','touch '.$unixmythtv.'mythtv/last_build.txt','cat '.$unixmythtv.'mythtv/svn_info.new >'.$unixmythtv.'mythtv/svn_info.txt','touch -r '.$unixmythtv.'mythtv/svn_info.txt '.$unixmythtv.'mythtv/svn_info.new'], comment => 'if the SVN number is changed, then remember that, AND arrange for a full re-make of mythtv. (overkill, I know, but safer)' ], 

# apply any outstanding win32 patches - this section will be hard to keep up
# with HEAD/SVN:

# 15586 and earlier need this patch:
#[ archive => $sources.'backend.patch.gz' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4392/backend.patch.gz', comment => 'backend.patch.gz - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/backend.patch.gz',$sources."backend.patch.gz"], copy => [''=>'',comment => '4392: - backend connections being accepted patch '] ],
#[ grep => ['unsigned\* Indexes = new unsigned\[n\]\;',$mythtv.'mythtv/libs/libmyth/mythsocket.cpp'], shell => ["cd ".$unixmythtv."mythtv/","gunzip -f backend.patch.gz","patch -p0 < backend.patch"] ],

# these next 3 patches are needed for 15528 (and earlier)
#[ archive => $sources.'importicons_windows_2.diff' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/3334/importicons_windows_2.diff', comment => 'importicons_windows_2.diff - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/importicons_windows_2.diff',$sources."importicons_windows_2.diff"], copy => [''=>'',comment => '3334 fixes error with mkdir() unknown.'] ],
#
#[ grep => ['\#include <qdir\.h>',$mythtv.'mythtv/libs/libmythtv/importicons.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < importicons_windows_2.diff"] ],
#[ archive => $sources.'mingw.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4516/mingw.patch', comment => 'mingw.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/mingw.patch',$sources."mingw.patch"], copy => [''=>'',comment => '4516 fixes build'] ],
#[ grep => ['LIBS \+= -lmyth-\$\$LIBVERSION',$mythtv.'mythtv/libs/libmythui/libmythui.pro'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < mingw.patch"] ]
#
# (fixed in 15547)
#[ archive => $sources.'util_win32.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4497/util_win32.patch', comment => 'util_win32.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/util_win32.patch',$sources."util_win32.patch"], copy => [''=>'',comment => '4497 fixes build'] ],
#[ grep => ['\#include "compat.h"',$mythtv.'mythtv/libs/libmyth/util.h'], shell => ["cd ".$unixmythtv."mythtv/libs/libmyth/","patch -p0 < ".$unixmythtv."mythtv/util_win32.patch"] ],

# post 15528, pre 15568  needs this: equivalent to: svn merge -r 15541:15540 .
#[ archive => $sources.'15541_undo.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/XXXX/15541_undo.patch', comment => 'util_win32.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/15541_undo.patch',$sources."15541_undo.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['\#include \"compat.h\"',$mythtv.'mythtv/libs/libmythui/mythpainter.cpp'], shell => ["cd ".$unixmythtv."mythtv/libs/libmyth/","patch -p2 < ".$unixmythtv."mythtv/15541_undo.patch"] , comment => 'currently need this patch too, equivalemnt of: svn merge -r 15541:15540 .'],

# [16167] needs these 4 to run:  
#4724 http://svn.mythtv.org/trac/raw-attachment/ticket/4724/win32_storeconnection.patch 
[ archive => $mythtv.'mythtv/win32_storeconnection.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4724/win32_storeconnection.patch', comment => 'win32_storeconnection.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'], 
[ file => $mythtv.'_', shell => ["cd ".$unixmythtv."mythtv/libs/libmyth","patch -p0 < ".$unixmythtv."mythtv/win32_storeconnection.patch","nocheck"] , comment => 'apply above patch'], 
#4706 http://svn.mythtv.org/trac/raw-attachment/ticket/4706/mythsocket_win32_events.patch 
[ archive => $mythtv.'mythtv/mythsocket_win32_events.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4706/mythsocket_win32_events.patch', comment => 'mythsocket_win32_events.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'], 
[ file => $mythtv.'_', shell => ["cd ".$unixmythtv."mythtv","patch -p0 < ".$unixmythtv."mythtv/mythsocket_win32_events.patch","nocheck"] , comment => 'apply above patch'], 
#4699 http://svn.mythtv.org/trac/raw-attachment/ticket/4699/win32_fs.patch 
[ archive => $mythtv.'mythtv/win32_fs.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4699/win32_fs.patch', comment => 'win32_fs.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'], 
[ file => $mythtv.'_', shell => ["cd ".$unixmythtv."mythtv","patch -p0 < ".$unixmythtv."mythtv/win32_fs.patch","nocheck"] , comment => 'apply above patch'], 
#4702 http://svn.mythtv.org/trac/raw-attachment/ticket/4702/mingw.patch 
[ archive => $mythtv.'mythtv/mingw.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4702/mingw.patch', comment => 'mingw.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'], 
[ file => $mythtv.'_', shell => ["cd ".$unixmythtv."mythtv","patch -p0 < ".$unixmythtv."mythtv/mingw.patch","nocheck"] , comment => 'apply above patch'], 
	 

[ file => $mythtv.'mythtv/config/config.pro', shell => ['touch '.$unixmythtv.'mythtv/config/config.pro'], comment => 'create an empty config.pro or the mythtv build will fail'],

# do a make clean before? Yes, if the SVN revision has changed (it deleted the file for us), or the user deleted the file manually, to request it. 
[ file => $mythtv.'delete_to_do_make_clean.txt', shell => ['source '.$unixmythtv.'qt_env.sh','cd '.$unixmythtv.'mythtv','make clean','find . -type f -name \*.dll | grep -v run | xargs -n1 rm','find . -type f -name \*.a | grep -v run | xargs -n1 rm','touch '.$unixmythtv.'delete_to_do_make_clean.txt','nocheck'], comment => 'do a "make clean" first? not strictly necessary in all cases, and the build will be MUCH faster without it, but it is safer with it... ( we do a make clean if the SVN revision changes) '], 

#broken Makefile, delete it
[ grep => ['Makefile|MAKEFILE',$mythtv.'mythtv/Makefile'], shell => ['rm '.$unixmythtv.'mythtv/Makefile','nocheck'], comment => 'broken Makefile, delete it' ],

# configure
[ file => $mythtv.'mythtv/Makefile', shell => ['source '.$unixmythtv.'qt_env.sh','cd '.$unixmythtv.'mythtv','./configure --prefix=/usr --disable-dbox2 --disable-hdhomerun --disable-iptv --disable-joystick-menu --disable-xvmc-vld --disable-xvmc --enable-directx --cpu=k8 --compile-type=debug'], comment => 'do we already have a Makefile for mythtv?' ],
# make
[ newer => [$mythtv."mythtv/libs/libmyth/libmyth-$version.dll",
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv."mythtv/libs/libmyth/libmyth-$version.dll",
            'source '.$unixmythtv.'qt_env.sh',
            'cd '.$unixmythtv.'mythtv','make'],
comment => "libs/libmyth/libmyth-$version.dll - redo make unless all these files exist, and are newer than the last_build.txt identifier" ],
[ newer => [$mythtv."mythtv/libs/libmythtv/libmythtv-$version.dll",
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv."mythtv/libs/libmythtv/libmythtv-$version.dll",
            'source '.$unixmythtv.'qt_env.sh',
            'cd '.$unixmythtv.'mythtv','make'],
comment => "libs/libmythtv/libmythtv-$version.dll - redo make unless all these files exist, and are newer than the last_build.txt identifier" ],
[ newer => [$mythtv.'mythtv/programs/mythfrontend/mythfrontend.exe',
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv.'mythtv/programs/mythfrontend/mythfrontend.exe',
            'source '.$unixmythtv.'qt_env.sh',
            'cd '.$unixmythtv.'mythtv','make'],
comment => 'programs/mythfrontend/mythfrontend.exe - redo make unless all these files exist, and are newer than the last_build.txt identifier' ],
[ newer => [$mythtv.'mythtv/programs/mythbackend/mythbackend.exe',
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv.'mythtv/programs/mythbackend/mythbackend.exe',
            'source '.$unixmythtv.'qt_env.sh',
            'cd '.$unixmythtv.'mythtv','make'],
comment => 'programs/mythbackend/mythbackend.exe - redo make unless all these files exist, and are newer than the last_build.txt identifier' ],

# re-install to msys /usr/bin folders etc, if we have a newer mythtv build
# ready:
[ newer => [$msys.'bin/mythfrontend.exe',
            $mythtv.'mythtv/programs/mythfrontend/mythfrontend.exe'],
  shell => ['source '.$unixmythtv.'qt_env.sh',
            'cd '.$unixmythtv.'mythtv','make install'],
comment => 'was the last configure successful? then install mythtv ' ],

# install some themes? does a 'make install' do that adequately (no, not if
# running outside msys)?
# copy the basic themes somewhere that mythtv can get at it.
# TODO this should really be independent of the msys folders, but it's not
# at present?

[ dir => $msys.'share\mythtv\themes\G.A.N.T', shell => ['mkdir /usr/share/mythtv','mkdir /usr/share/mythtv/themes','cp -r /c/mythtv/mythtv/themes/* /usr/share/mythtv/themes/'], comment => 'copy the basic themes somewhere that mythtv can get at it.' ],

# 
[ file => $mythtv.'make_run.sh_', write => [$mythtv.'make_run.sh',
'#!/bin/bash
source '.$unixmythtv.'qt_env.sh
cd '.$unixmythtv.'mythtv
# keep around just one earlier version in run_old:
rm -rf run_old
mv run run_old
mkdir run
# copy exes and dlls to the run folder:
find . -name \\*.exe  | grep -v run | xargs -n1 -i__ cp __ ./run/
find . -name \\*.dll  | grep -v run | xargs -n1 -i__ cp __ ./run/  
# mythtv needs the qt dlls at runtime:
cp '.$unixmsys.'qt-3.3.x-p8/lib/*.dll '.$unixmythtv.'mythtv/run
# qt mysql connection dll has to exist in a subfolder called sqldrivers:
mkdir '.$unixmythtv.'mythtv/run/sqldrivers
cp '.$unixmsys.'qt-3.3.x-p8/plugins/sqldrivers/libqsqlmysql.dll '.$unixmythtv.'mythtv/run/sqldrivers 
# pthread dlls and mingwm10.dll are copied from here:
cp /mingw/bin/*.dll '.$unixmythtv.'mythtv/run
cp /bin/msys*.dll '.$unixmythtv.'mythtv/run
cp '.$unixmythtv.'mythtv/contrib/Win32/debug/*.cmd '.$unixmythtv.'mythtv/run
','nocheck' 
],comment => 'write a script that will copy all the files necessary for running mythtv out of the build folder into the ./run folder'],


[ newer => [$mythtv.'mythtv/run/mythfrontend.exe',$mythtv.'mythtv/programs/mythfrontend/mythfrontend.exe'], shell => [$unixmythtv.'make_run.sh'], comment => 'create a natively runnable version: with the make_run.sh script' ],

# create the mysql.txt file at: %HOMEPATH%\.mythtv\mysql.txt
[ file => $ENV{HOMEPATH}.'\.mythtv\mysql.txt_', write => [$ENV{HOMEPATH}.'\.mythtv\mysql.txt',
'#
DBHostName=127.0.0.1
DBHostPing=no
DBUserName=mythtv
DBPassword=mythtv
DBName=mythconverg
DBType=QMYSQL3
#LocalHostName=my-unique-identifier-goes-here
','nocheck'],
comment => 'create a mysql.txt file at: %HOMEPATH%\.mythtv\mysql.txt' ],

 
#execute and capture output: C:\Program Files\MySQL\MySQL Server 5.0\bin>mysqlshow.exe -u mythtv --password=mythtv
# example response: mysqlshow.exe: Can't connect to MySQL server on 'localhost' (10061)
# if this is doing an anonymous connection, so the BEST we should expect is
# an "access denied" message if the server is running properly.

[ file => $mythtv.'testmysql.bat_', write => [ $mythtv.'testmysql.bat',
'@echo off
echo testing connection to a local mysql server...
sleep 5
del '.$dosmythtv.'_mysqlshow_err.txt
"C:\Program Files\MySQL\MySQL Server 5.0\bin\mysqlshow.exe" -u mythtv --password=mythtv 2> '.$dosmythtv.'_mysqlshow_err.txt  > '.$dosmythtv.'_mysqlshow_out.txt 
type '.$dosmythtv.'_mysqlshow_out.txt >> '.$dosmythtv.'_mysqlshow_err.txt 
del '.$dosmythtv.'_mysqlshow_out.txt
sleep 1
','nocheck']],

# try to connect as mythtv/mythtv first (the best case scenario)
[ file => $mythtv.'skipping_db_tests.txt', exec => [$mythtv.'testmysql.bat','nocheck'], comment => 'First check - is the local mysql server running, accepting connections, and all that? (follow the bouncing ball on the install, a standard install is OK, remember the root password that you set, start it as a service!)' ],

# if the connection was good, or the permissions were wrong, but the server
# was there, there's no need to reconfigure the server!
[ grep => ['(\+--------------------\+|Access denied for user)',$mythtv.'_mysqlshow_err.txt'], exec => ['C:\Program Files\MySQL\MySQL Server 5.0\bin\MySQLInstanceConfig.exe','nocheck'], comment => 'See if we couldnt connect to a local mysql server. Please re-configure the MySQL server to start as a service.'],

# try again to connect as mythtv/mythtv first (the best case scenario) - the
# connection info should have changed!
[ file => $mythtv.'skipping_db_tests.txt', exec => [$mythtv.'testmysql.bat','nocheck'], comment => 'Second check - that the local mysql server is at least now now running?' ],


#set/reset mythtv/mythtv password! 
[ file => $mythtv.'resetmythtv.bat_', write => [ $mythtv.'resetmythtv.bat',
"\@echo off
echo stopping mysql service:
net stop MySQL
sleep 2
echo writing script to reset mythtv/mythtv passwords:
echo USE mysql; >resetmythtv.sql
echo. >>resetmythtv.sql
echo INSERT IGNORE INTO user VALUES ('localhost','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo REPLACE INTO user VALUES ('localhost','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo INSERT IGNORE INTO user VALUES ('\%\%','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo REPLACE INTO user VALUES ('\%\%','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo trying to reset mythtv/mythtv passwords:
\"C:\\Program Files\\MySQL\\MySQL Server 5.0\\bin\\mysqld-nt.exe\" --no-defaults --bind-address=127.0.0.1 --bootstrap --console --skip-grant-tables --skip-innodb --standalone <resetmythtv.sql
del resetmythtv.sql
echo trying to re-start mysql
rem net stop MySQL
net start MySQL
rem so that the server has time to start before we query it again
sleep 5
echo.
echo Password for user 'mythtv' was reset to 'mythtv'
echo.
",'nocheck'],
comment => 'writing a script to create the mysql user (mythtv/mythtv) without needing to ask you for the root password ...'],

# reset passwords, this give the myhttv user FULL access to the entire mysql
# instance!
# TODO give specific access to just the the mythconverg DB, as needed.
[ grep => ['(\+--------------------\+)',$mythtv.'_mysqlshow_err.txt'], exec => [$dosmythtv.'resetmythtv.bat','nocheck'], comment => 'Resetting the mythtv/mythtv permissions to mysql - if the user already has successful login access to the mysql server, theres no need to run this' ],


# try again to connect as mythtv/mythtv first (the best case scenario) - the
# connection info should have changed!
[ file => $mythtv.'skipping_db_tests.txt', exec => [$mythtv.'testmysql.bat','nocheck'], comment => 'Third check - that the local mysql server is fully accepting connections?' ],

# create DB:
# this has the 'nocheck' flag because the creation of the DB doesn't
# instantly reflect in the .txt file we are looking at:
[ grep => ['mythconverg',$mythtv.'_mysqlshow_err.txt'], exec => [ 'echo create database mythconverg; | "C:\Program Files\MySQL\MySQL Server 5.0\bin\mysql.exe" -u mythtv --password=mythtv','nocheck'], comment => ' does the mythconverg database exist? (and can this user see it?) '],

#----------------------------------------

#  build the mythplugins now:
# 

# get mythtv sources, if we don't already have them
# download all the files from the web, and save them here:
#[ dir => $mythtv.'mythplugins', mkdirs => $mythtv.'mythplugins' ],
#
## config:
#[ file => $mythtv.'mythplugins/Makefile', shell => ['source '.$unixmythtv.'qt_env.sh','cd '.$unixmythtv.'mythplugins','./configure --prefix=/usr --disable-mythgallery --disable-mythmusic --disable-mytharchive --disable-mythbrowser --disable-mythflix --disable-mythgame --disable-mythnews --disable-mythphone --disable-mythzoneminder --disable-mythweb --enable-aac --enable-libvisual --enable-fftw --compile-type=debug'], comment => 'do we already have a Makefile for myth plugins?' ],
## make
#[ newer => [$mythtv.'mythplugins/mythmovies/mythmovies/libmythmovies.dll',$mythtv.'mythtv/last_build.txt'], shell => ['rm '.$unixmythtv.'mythplugins/mythmovies/mythmovies/libmythmovies.dll','source '.$unixmythtv.'qt_env.sh','cd '.$unixmythtv.'mythplugins','make'], comment => 'PLUGINS! redo make if we need to (see the  last_build.txt identifier)' ],


# re-install to msys /usr/bin folders etc, if we have a newer mythtv build
# ready:
#[ newer => [$msys.'bin/mythfrontend.exeXXXXX',$mythtv.'mythplugins/mythmovies/mythmovies/XXXXX'], shell => ['source '.$unixmythtv.'qt_env.sh','cd '.$unixmythtv.'mythplugins','make install'], comment => 'plugins - was the last configure successful? then install mythtv ' ],



#----------------------------------------

; # END OF CAUSE->ACTION DEFINITIONS

#------------------------------------------------------------------------------

sub _end {
	
	comment("This version of the Win32 Build script last was last tested on: $SVNRELEASE");

print << 'END';    
#
# SCRIPT TODO/NOTES:  - further notes on this scripts direction....
# * if the build was successful then try running the 'mythfrontend.exe' and 'mythbackend.exe' from the 'C:/mythtv/mythtv/run' folder.
# * ok, how about the test-run process?  
# * check that mythtv/mythtv/mythconverg will access the mysql database
# * mythplugins build isn't currently working, so disabled.
# * myththemes build isn't tried at all
END
}

#------------------------------------------------------------------------------

# this is the mainloop that iterates over the above definitions and
# determines what to do:
# cause:
foreach my $dep ( @{$expect} ) { 
    my @dep = @{$dep};

    #print Dumper(\@dep);

    my $causetype = $dep[0];
    my $cause =  $dep[1];
    my $effecttype = $dep[2];
    my $effectparams = $dep[3] || '';
    die "too many parameters in cause->event declaration (".join('|',@dep).")" if defined $dep[4] && $dep[4] ne 'comment';  # four pieces: cause => [blah] , effect => [blah]
    my $comment = $dep[5] || '';

    if ( $comment && $NOISY ) {
        comment($comment);
    }

    my @cause;
    if (ref($cause) eq "ARRAY" ) {
        @cause = @{$cause};
    } else { 
        push @cause, $cause ;
    }

    # six pieces: cause => [blah] , effect => [blah] , comment => ''
    die "too many parameters in cause->event declaration (@dep)"
        if defined $dep[6];

    my @effectparams = ();
    if (ref($effectparams) eq "ARRAY" ) {
        @effectparams = @{$effectparams};
    } else { 
        push @effectparams, $effectparams ;
    }
    # if a 'nocheck' parameter is passed through, dont pass it through to
    # the 'effect()', use it to NOT check if the file/dir exists at the end.
    my @nocheckeffectparams = grep { ! /nocheck/i } @effectparams; 
    my $nocheck = 0;
    if ( $#nocheckeffectparams != $#effectparams ) { $nocheck = 1; } 

    if ( $causetype eq 'archive' ) {
        die "archive only supports type fetch ($cause)($effecttype)"
            unless $effecttype eq 'fetch';
        if ( -f $cause[0] ) {print "file exists: $cause[0]\n"; next;}
        # 2nd and 3rd params get squashed into
        # a single array on passing to effect();
        effect($effecttype,$cause[0],@nocheckeffectparams);
        if ( ! -f $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to locate expected file ($cause[0]) that was to be fetched from $nocheckeffectparams[0]\n";
        }

    }   elsif ( $causetype eq 'dir' ) {
        if ( -d $cause[0] ) {
            print "directory exists: $causetype,$cause[0]\n"; next;
        }
        effect($effecttype,@nocheckeffectparams);
        if ( ! -d $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to locate expected directory ($cause[0]).\n";
        }

    } elsif ( $causetype eq 'file' ) {
        if ( -f $cause[0] ) {print "file exists: $cause[0]\n"; next;}
        effect($effecttype,@nocheckeffectparams);
        if ( ! -f $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to locate expected file ($cause[0]).\n";
        }
    } elsif ( $causetype eq 'filesame' ) {
        # TODO - currently we check file mtime, and byte size, but not
        # MD5/CRC32 or contents, this is "good enough" for most
        # circumstances.
        my ( $size,$mtime)=(0,0);
        if ( -f $cause[0] ) {
          $size = (stat($cause[0]))[7];
          $mtime  = (stat($cause[0]))[9];
        }
        if (! (-f $cause[1] ) ) {  die "cause: $causetype requires its SECOND filename to exist for comparison: ($cause[1]).\n"; }
        my $size2 = (stat($cause[1]))[7];
        my $mtime2  = (stat($cause[1]))[9];
        if ( $mtime != $mtime2 || $size != $size2) {
          if ( ! $nocheckeffectparams[0] ) {
            die "sorry but you can not leave the arguments list empty for anything except the 'copy' action (and only when used with the 'filesame' cause)" unless $effecttype eq 'copy';
            print "no parameters defined, so applying effect($effecttype) as ( 2nd src parameter => 1st src parameter)!\n";
            effect($effecttype,$cause[1],$cause[0]); #copy the requested file[1] to the requested destn[0], now the [0] file exists and is the same.
          } else {
            effect($effecttype,@nocheckeffectparams); # do something else if the files are not 100% identical?
          }  
        }else {
           print "effect not required files already up-to-date/identical: ($cause[0] => $cause[1]).\n";
        }
        undef $size; undef $mtime;
        undef $size2; undef $mtime2;
        
    } elsif ( $causetype eq 'newer' ) {
        my $mtime = 0;
        if ( -f $cause[0] ) {
          $mtime   = (stat($cause[0]))[9];
        }
        if (! ( -f $cause[1]) ) {  die "cause: $causetype requires it's SECOND filename to exist for comparison: $cause[1].\n"; }
        my $mtime2  = (stat($cause[1]))[9];
        if ( $mtime < $mtime2 ) {
          effect($effecttype,@nocheckeffectparams);
          if ( $nocheck == 0 ) {
            # confirm it worked, mtimes should have changed now: 
            my $mtime3 = 0;
            if ( -f $cause[0] ) {
              $mtime3   = (stat($cause[0]))[9];
            }
            my $mtime4  = (stat($cause[1]))[9];
            if ( $mtime3 < $mtime4  ) {
                die "EFFECT FAILED ($causetype -> $effecttype): mtime of file ($cause[0]) should be greater than file ($cause[1]).\n";
            }
          }
        } else {
           print "file ($cause[0]) has same or newer mtime than ($cause[1]) already, no action taken\n";
        } 
        undef $mtime;
        undef $mtime2;
        
    } elsif ( $causetype eq 'grep' ) {
        if ( ! _grep($cause[0],$cause[1]) ) { # grep actually needs two parameters on the source side of the action
            effect($effecttype,@nocheckeffectparams);   
        } else {
            print "grep - already matched source file($cause[1]), with pattern ($cause[0]) - no action reqd\n";
        }
        if ( (! _grep($cause[0],$cause[1])) && $nocheck == 0 ) { 
           die "EFFECT FAILED ($causetype -> $effecttype): unable to locate regex pattern ($cause[0]) in file ($cause[1])\n";
        }

    } else {
        die " unknown causetype $causetype \n";
    }
}

print "\nall done\n";

_end();

#------------------------------------------------------------------------------

# each cause has an effect, this is where we do them:
sub effect {
    my ( $effecttype, @effectparams ) = @_;

        if ( $effecttype eq 'fetch') {
            # passing two parameters that came in via the array
            _fetch(@effectparams);

        } elsif ( $effecttype eq 'extract') {
            my $tarfile = $effectparams[0];
            my $destdir = $effectparams[1] || '';
            if ($destdir eq '') {
                $destdir = $tarfile;
                # strip off everything after the final forward slash
                $destdir =~ s#[^/]*$##;
            }
        my $t = findtar($tarfile);
        print "found equivalent: ($t) -> ($tarfile)\n" if $t ne $tarfile;
        print "extracttar($t,$destdir);\n";
        extracttar($t,$destdir);

        } elsif ($effecttype eq 'exec') { # execute a DOS command
            my $cmd = shift @effectparams;
            #print `$cmd`;
            print "exec:$cmd\n";
            open F, $cmd." |"  || die "err: $!";
            while (<F>) {
                print;
            }   

        } elsif ($effecttype eq 'shell') {
            shell(@effectparams);
            
        } elsif ($effecttype eq 'copy') {
            die "Can not copy non-existant file ($effectparams[0])\n" unless -f $effectparams[0];
            print "copying file ($effectparams[0] => $effectparams[1]) \n";
            cp($effectparams[0],$effectparams[1]);
            # make destn mtime the same as the original for ease of comparison:
            shell("touch --reference=".perl2unix($effectparams[0])." ".perl2unix($effectparams[1]));

        } elsif ($effecttype eq 'mkdirs') {
            mkdirs(shift @effectparams);

        } elsif ($effecttype eq 'write') {
            # just dump the requested content from the array to the file.
            my $filename = shift @effectparams;
            my $fh = new IO::File ("> $filename")
                || die "error opening $filename for writing: $!\n";
            $fh->binmode();
            $fh->print(join('',@effectparams));
            $fh->close();

        } else {
            die " unknown effecttype $effecttype from cause 'dir'\n";
        }
}
#------------------------------------------------------------------------------

# kinda like a directory search for blah.tar* but faster/easier.
#  only finds .tar.gz, .tar.bz2, .zip
sub findtar {
    my $t = shift;
    return "$t.gz" if -f "$t.gz";
    return "$t.bz2" if -f "$t.bz2";

    if ( -f "$t.zip" || $t =~ m/\.zip$/ ) {
        die "no unzip.exe found ! - yet" unless -f $sources."unzip/unzip.exe";
        # TODO - a bit of a special test, should be fixed better.
        return "$t.zip" if  -f "$t.zip";
        return $t if -f $t;
    }
    return $t if -f $t;
    die "findtar failed to match a file from:($t)\n";
}
#------------------------------------------------------------------------------


# given a ($t) .tar.gz, .tar.bz2, .zip extract it to the directory ( $d)
# changes current directory to $d too
sub extracttar {
    my ( $t, $d) = @_;

    # both $t and $d at this point should be perl-compatible-forward-slashed
    die "extracttar expected forward-slashes only in pathnames ($t,$d)"
        if $t =~ m#\\# || $d =~ m#\\#;

    unless ( $t =~ m/zip/ ) {
        # the unzip tool need the full DOS path,
        # the msys commands need that stripped off.
        $t =~ s#^$msys#/#i;
    }

    print "extracting to: $d\n";

    # unzipping happens in DOS as it's a dos utility:
    if ( $t =~ /\.zip$/ ) {
        #$d =~ s#/#\\#g;  # the chdir command MUST have paths with backslashes,
                          # not forward slashes. 
        $d = perl2dos($d);
        $t = perl2dos($t);
        my $cmd = 'chdir '.$d.' && '.$dossources.'unzip\unzip.exe -o '.$t;
        print "extracttar:$cmd\n";
        open F, "$cmd |"  || die "err: $!";
        while (<F>) {
            print;
        }
        return; 
    }

    $d = perl2unix($d);
    $t = perl2unix($t);
    # untarring/gzipping/bunzipping happens in unix/msys mode:
    die "unable to locate sh2.exe" unless -f $dosmsys.'bin/sh2.exe';
    my $cmd = $dosmsys.'bin\sh2.exe -c "( export PATH=/bin:/mingw/bin:$PATH ; ';
    $cmd .= "cd $d;";
    if ( $t =~ /\.gz$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -zxvpf $t";
    } elsif ( $t =~ /\.bz2$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -jxvpf $t";
    } elsif ( $t =~ /\.tar$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -xvpf $t";
    } else {
        die  "extract tar failed on ($t,$d)\n";
    }
    $cmd .= ')"'; # end-off the brackets around the shell commands.

    # execute the cmd, and capture the output!  
    # this is a glorified version of "print `$cmd`;"
    # except it doesn't buffer the output, if $|=1; is set.
    # $t should be a msys compatible path ie /sources/etc
    print "extracttar:$cmd\n";
    open F, "$cmd |"  || die "err: $!";
    while (<F>) {
        print;
    }   
}
#------------------------------------------------------------------------------


# get the $url (typically a .tar.gz or similar) , and save it to $file
sub _fetch {
    my ( $file,$url ) = @_;

    #$file =~ s#/#\\\\#g;
    print "already exists: $file \n" if -f $file;
    return undef if -f $file;

    print "fetching $url to $file (please wait)...\n";
    my $ua = LWP::UserAgent->new;
    $ua->proxy(['http', 'ftp'], $proxy);

    my $req = HTTP::Request->new(GET => $url);
    my $res = $ua->request($req);

    if ($res->is_success) {
        my $f = new IO::File "> $file" || die "_fetch: $!\n";
        $f->binmode();
        $f->print($res->content);
        $f->close();
    }
    if ( ! -s $file ) {
      die ("ERR: Unable to automatically fetch file!\nPerhaps manually downloading from the URL to the filename (both listed above) might work, or you might want to choose your own SF mirror (edit this script for instructions), or perhaps this version of the file is no longer available.");
    }
}
#------------------------------------------------------------------------------


# execute a sequence of commands in a bash shell.
# we explicitly add the /bin and /mingw/bin to the path
# because at this point they aren't likely to be there
# (cause we are in the process of installing them)
sub shell {
    my @cmds = @_;
    my $cmd = $dosmsys.'bin\bash.exe -c "( export PATH=/bin:/mingw/bin:$PATH;'.join(';',@cmds).') 2>&1 "';
    print "shell:$cmd\n";
    # execute the cmd, and capture the output!  this is a glorified version
    # of "print `$cmd`;" except it doesn't buffer the output if $|=1; is set.
    open F, "$cmd |"  || die "err: $!";
    while (<F>) {
        if (! $NOISY )  {
          # skip known spurious messages from going to the screen unnecessarily
          next if /redeclared without dllimport attribute after being referenced with dllimpo/;
          next if /warning: overriding commands for target `\.\'/;
          next if /warning: ignoring old commands for target `\.\'/;
          next if /Nothing to be done for `all\'/;
          next if /^cd .* \&\& \\/;
          next if /^make -f Makefile/;
        }
        print;
    }
}
#------------------------------------------------------------------------------
# recursively make folders, requires perl-compatible folder separators
# (ie forward slashes)
# 
sub mkdirs {
    my $path = shift;
    die "backslash in foldername not allowed in mkdirs function:($path)\n"
        if $path =~ m#\\#;
    $path = perl2dos($path);
    print "mkdir $path\n";
    # reduce path to just windows,
    # incase we have a rogue unix mkdir command elsewhere!
    print `set PATH=C:\\WINDOWS\\system32;c:\\WINDOWS;C:\\WINDOWS\\System32\\Wbem && mkdir $path`;

}

#------------------------------------------------------------------------------


sub perl2unix  {
    my $p = shift;
    $p =~ s#$msys#/#i;  # remove superfluous msys folders if they are there
    $p =~ s#^([CD]):#/$1#ig;  #change c:/ into /c  (or a D:)   so c:/msys becomes /c/msys etc.
    $p =~ s#//#/#ig; # reduce any double forward slashes to single ones.
    return $p;
}
# DOS executable CMD.exe versions of the paths (for when we shell to DOS mode):
sub perl2dos {
    my $p = shift;
    $p =~ s#/#\\#g; # single forward to single backward
    return $p;
}
#------------------------------------------------------------------------------

sub _grep {
    my ($pattern,$file) = @_;
    #$pattern = qw($pattern);
    print "grep-ing for pattern($pattern) in file($file)\n";
    my $fh = IO::File->new("< $file");
    unless ( $fh) { print "WARNING: Unable to read file ($file) when searching for pattern:($pattern), assumed to NOT match pattern\n";  return 0; }
    my $found = 0;
    while ( my $contents = <$fh> ) {
        if ( $contents =~ m/$pattern/ ) { $found= 1; }
    }
    $fh->close();
    return $found;
}
#------------------------------------------------------------------------------

sub comment {
	      my $comment = shift;
				print "\nCOMMENTS:";
        print "-"x30;
        print "\n";
        print "COMMENTS:$comment\nCOMMENTS:";
        print "-"x30;
        print "\n";
        print "\n";
}
