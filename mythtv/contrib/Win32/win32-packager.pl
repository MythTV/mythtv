#!c:/perl/bin/perl.exe -w
##############################################################################
### =file
### win32-packager.pl
###
### =description
### Tool for automating frontend builds on MS WIndows XP (and compatible)
### based loosely on osx-packager.pl
###
### =revision
### $Id$
###
### =author
### David Bussenschutt
##############################################################################

use strict;
#use Getopt::Long qw(:config auto_abbrev);
#use Pod::Usage ();
#use Cwd ();
use LWP::UserAgent;
use IO::File;
use Data::Dumper; 

my $NOISY = 1; # set to 0 for less output to the screen

$| = 1; # autoflush stdout;

my $SVNRELEASE = '15367' ;# this scipt was last tested to work with this version, on other versions YMMV.

# We allow SourceForge to tell us which server to download from,
# rather than assuming specific server/s
my $sourceforge = 'downloads.sourceforge.net';     # auto-redirect to a
                                                   # mirror of SF's choosing,
                                                   # hopefully close to you
# alternatively you can choose your own mirror:
#my $sourceforge = 'optusnet.dl.sourceforge.net';  # australia  <- the author
#                                                  #            uses this one! 
#my $sourceforge = 'internap.dl.sourceforge.net';  # USA,California
#my $sourceforge = 'easynews.dl.sourceforge.net';  # USA,Arizona,Phoenix,
#my $sourceforge = 'jaist.dl.sourceforge.net';     # Japan
#my $sourceforge = 'mesh.dl.sourceforge.net';      # Germany

# Set this to the empty string for no-proxy:
# TODO - proxy code is broken for SVN actions,
#        however downloads/etc will still work fine.
# TODO using the proxy here WILL cause any Subversion (SVN) commands to fail,
#      you will need to do that by hand.
my $proxy = '';
#my $proxy = 'http://prx-bne.qmtechnologies.com:8080';

# TODO -  use this list to define the components to build - 
my @components = ( 'mythtv', 'myththemes', 'mythplugins' );


# TODO - we should try to autodetect these paths, rather than assuming
#        the defaults - perhaps from environment variables like this:
#  die "must have env variable SOURCES pointing to your sources folder"
#      unless $ENV{SOURCES};
#  my $sources = $ENV{SOURCES};
# TODO - although theoretically possible to change these paths,
#        it has NOT been tested, and will with HIGH PROBABILITY fail somewhere

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
my $unixmingw = '/mingw/'; # mingw is always mounted here under unix, if you setup mingw right in msys.  (see /etc/fstab)
my $unixsources = perl2unix($sources); $unixsources =~ s#$unixmsys#/#i;  #strip leading msys path, if there, it's unnecessary as it's mounted under /
my $unixmythtv  = perl2unix($mythtv);


#NOTE: ITS IMPORTANT THAT ALL PATHS use FORWARD SLASHES in the declarations below, the code depends on it.
#      ... and much of the path separator code is fragile!
#      In some places we regex these into double backslashes, and at other times to a single 
#      backslash so that the call-outs to different environments (DOS/msys/perlticks) work.  hopefully.  
#      (The only exception to "always forwardslash" is the contents of the file/s that are created with the 'write' action.)

#NOTE: [exec]   actions should always refer to  $dosXXX paths
#      [shell]  actions should always refer to $unixXXX paths
#      [dir],[file],[mkdirs],[archive] actions should always refer to default perl compatible paths

# NOTE:  The architecture of this script is based on cause-and-event.  
#        There are a number of "causes" (or expectations) that can trigger an event/action.
#        There are a number of different actions that can be taken.
#
# eg: [ dir  => "c:/MinGW", exec => $sources.'MinGW-5.1.3.exe' ],
#
# means: expect there to be a dir called "c:/MinGW", and if there isn't execute the file MinGW-5.1.3.exe.
# (clearly there needs to be a file MinGW-5.1.3.exe on disk for that to work, so there is an earlier declaration to 'fetch' it)


#build expectations (causes) :
#  missing a file (given an expected path)                                 [file]
#  missing folder                                                          [dir]
#  missing source archive (fancy version of 'file' to fetch from the web)  [archive]
#  apply a perl pattern match and if it DOESNT match execute the action    [grep]  - this 'cause' actually needs two parameters in an array [ pattern, file]

#build actions (events) are:
#  fetch a file from the web (to a location)                         [fetch]
#  set an environment variable (name, value pair)                    not-yet-impl
#  execute a DOS/Win32 exe/command and wait to complete              [exec]
#  execute a MSYS/Unix script/command in bash and wait to complete   [shell]- this 'effect' actually accepts many parameters in an array [ cmd1, cmd2, etc ]
#  extract a .tar .tar.gz or .tar.bz2 or ,zip file ( to a location)  [extract] - (note that .gz and .bz2 are thought equivalent)
#  write a small patch/config/script file directly to disk           [write]
#  make directory tree upto the path specified                       [mkdirs]
#TODO:
#  copy a file or set of files (path/filespec,  destination)         not-yet-impl.  use exec => 'copy /Y xxx yyy'
#  apply a diff                                                      not-yet-impl   use shell => 'patch -p0 < blah.patch'
#  search-replace text in a file                                     not-yet-impl


# NOTES on specific actions: 
# 'extract' now requires a all paths to be perl compatible (like all other commands)  If not supplied, it extracts into the folder the .tar.gz is in. 
# 'exec' actually runs all your commands inside a bash shell with -c "( cmd;cmd;cmd )" so be careful about quoting.


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


[ dir  => $mingw, exec => $sources.'MinGW-5.1.3.exe',comment => 'install MinGW (be sure to install g++, g77 and ming make too) - it will require user interaction, but once completed, will return control to us....' ], # interactive, supposed to install g++ and ming make too, but people forget to select them? 
[ file  => $mingw."bin/gcc.exe", exec => $sources.'MinGW-5.1.3.exe',comment => 'unable to gind a gcc.exe where expected, rerunning MinGW installer!' ], # interactive, supposed to install g++ and ming make too, but people forget to select them? 

[ archive => $sources.'MSYS-1.0.10.exe', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/MSYS-1.0.10.exe',comment => 'Get the MSYS and addons:' ] ,
[ archive => $sources.'bash-3.1-MSYS-1.0.11-1.tar.bz2', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/bash-3.1-MSYS-1.0.11-1.tar.bz2' ] ,
[ archive => $sources.'zlib-1.2.3-MSYS-1.0.11.tar.bz2', 'fetch' => 'http://easynews.dl.sourceforge.net/sourceforge/mingw/zlib-1.2.3-MSYS-1.0.11.tar.bz2' ] ,
[ archive => $sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2', 'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2' ] ,

# install MSYS, it supplies the 'tar' executable, among others:
[ file => $msys.'bin/tar.exe', 'exec' => $sources.'MSYS-1.0.10.exe',comment => 'Install MSYS, it supplies the tar executable, among others. You should follow prompts, AND do post-install in DOS box.' ] , 

#  don't use the [shell] action here, as it's not available until bash is installed!
[ file => $msys.'bin/sh2.exe', exec => 'copy /Y '.$dosmsys.'bin/sh.exe '.$dosmsys.'bin/sh2.exe',comment => 'make a copy of the sh.exe so that we can utilise it when we extract later stuff' ],

# prior to this point you can't use the 'extract' feature, or the 'shell' feature!


# if you did a default-install of MingW, then you need to try again, as we really need g++ and mingw32-make, and g77 is needed for fftw
[ file => $mingw.'bin/mingw32-make.exe',  exec => $sources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],
[ file => $mingw.'bin/g++.exe', exec => $sources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],
[ file => $mingw.'bin/g77.exe', exec => $sources.'MinGW-5.1.3.exe',comment => 'Seriously?  You must have done a default install of MinGW.  go try again! You MUST choose the custom installed and select the mingw make, g++ and g77 optional packages.' ],

#[ file => 'C:/MinGW/bin/mingw32-make.exe',  extract => $sources.'mingw32-make-3.81-2.tar',"C:/MinGW" ], - optionally we could get mingw32-make from here

# now that we have the 'extract' feature, we can finish ...
[ file => $mingw.'/bin/reimp.exe',  extract => [$sources.'mingw-utils-0.3.tar', $mingw],comment => 'Now we can finish all the mingw and msys addons:' ],
[ file => $msys.'bin/bash.exe',  extract => [$sources.'bash-3.1-MSYS-1.0.11-1.tar', $msys] ],
[ dir => $sources.'coreutils-5.97',  extract => [$sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar'] ],
[ file => $msys.'bin/pr.exe', shell => ["cd ".$unixsources."coreutils-5.97","cp -r * / "] ],

[ dir => $msys."lib" ,  mkdirs => $msys.'lib' ],
[ dir => $msys."include" ,  mkdirs => $msys.'include' ],


# (alternate would be from the gnuwin32 project, which is actually from same source)
#  run it into a 'unzip' folder, becuase it doesn't extract to a folder:
[ dir => $sources."unzip" ,  mkdirs => $sources.'unzip',comment => 'unzip.exe - Get a precompiled native Win32 version from InfoZip' ],
[ archive => $sources.'unzip/unz552xN.exe',  fetch => 'ftp://tug.ctan.org/tex-archive/tools/zip/info-zip/WIN32/unz552xN.exe'],
[ file => $sources.'unzip/unzip.exe', exec => "cd ".$dossources."unzip/ && ".$dossources."unzip/unz552xN.exe " ],
# we could probably put the unzip.exe into the path...


[ archive => $sources.'svn-win32-1.4.6.zip',  fetch => 'http://subversion.tigris.org/files/documents/15/41077/svn-win32-1.4.6.zip',comment => 'Subversion comes as a zip file, so it cant be done earlier than the unzip tool!'],
[ dir => $sources.'svn-win32-1.4.6', extract => $sources.'svn-win32-1.4.6.zip' ],


[ file => $msys.'bin/svn.exe', shell => ["cp -R $unixsources/svn-win32-1.4.6/* ".$unixmsys],comment => 'put the svn.exe executable into the path, so we can use it later!' ],

# :
[ dir => $sources."zlib" ,  mkdirs => $sources.'zlib',comment => 'the zlib download is a bit messed-up, and needs some TLC to put everything in the right place' ],
[ dir => $sources."zlib/usr",  extract => [$sources.'zlib-1.2.3-MSYS-1.0.11.tar', $sources."zlib"] ],
# install to /usr:
[ file => $msys.'lib/libz.a',  exec => "copy /Y ".$dossources."zlib/usr/lib/* ".$dosmsys."lib/" ],
[ file => $msys.'bin/msys-z.dll',  exec => "copy /Y ".$dossources."zlib/usr/bin/* ".$dosmsys."bin/" ],
[ file => $msys.'include/zlib.h',  exec => "copy /Y ".$dossources."zlib/usr/include/* ".$dosmsys."include/" ],
# taglib also requires zlib in /mingw , so we'll put it there too, why not! 
[ file => $msys.'lib/libz.a',  exec => "copy /Y ".$dossources."zlib/usr/lib/* ".$dosmingw."lib/" ],
[ file => $msys.'bin/msys-z.dll',  exec => "copy /Y ".$dossources."zlib/usr/bin/* ".$dosmingw."bin/" ],
[ file => $msys.'include/zlib.h',  exec => "copy /Y ".$dossources."zlib/usr/include/* ".$dosmingw."include/" ],

# fetch mysql
# primary server site is: http://dev.mysql.com/get/Downloads/MySQL-5.0/mysql-essential-5.0.45-win32.msi/from/http://mysql.mirrors.ilisys.com.au/
[ archive => $sources.'mysql-essential-5.0.45-win32.msi', 'fetch' => 'http://mirror.services.wisc.edu/mysql/Downloads/MySQL-5.0/mysql-essential-5.0.45-win32.msi',comment => 'fetch mysql binaries - this is a big download(23MB) so it might take a while' ],
[ file => "c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll", exec => $sources.'mysql-essential-5.0.45-win32.msi',comment => 'Install mysql - be sure to choose to do a "COMPLETE" install. You should also choose NOT to "configure the server now" ' ],

# after mysql install 
[ file => $mingw.'bin/libmySQL.dll',  exec => "copy /Y \"c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll\" $dosmingw/bin/",comment => 'post-mysql-install' ],
[ file => $mingw.'lib/libmySQL.dll',  exec => "copy /Y \"c:/Program Files/MySQL/MySQL Server 5.0/bin/libmySQL.dll\" $dosmingw/lib/",comment => 'post-mysql-install' ],
[ file => $mingw.'include/mysql.h',  exec => "copy /Y \"c:/Program Files/MySQL/MySQL Server 5.0/include/*\" $dosmingw/include/" ],
[ file => $mingw.'lib/libmysql.lib',  exec => "copy /Y \"c:/Program Files/MySQL/MySQL Server 5.0/lib/opt/libmysql.lib\" $dosmingw/lib/" ],
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/include/* /c/MinGW/include/
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/bin/libmySQL.dll /c/MinGW/lib
# cp /c/Program\ Files/MySQL/MySQL\ Server\ 5.0/lib/opt/libmysql.lib /c/MinGW/lib

# 
# TIP: we use a special file (with an extra _ ) as a marker to do this action every the time, it's harmless to do it more often that required. "nocheck" means continue even if the cause doesn't exist after.
[ file => $mingw.'lib/libmysql.lib__',  shell => ["cd $unixmingw/lib","reimp -d libmysql.lib","dlltool -k --input-def libmysql.def --dllname libmysql.dll --output-lib libmysql.a","nocheck"],comment => ' rebuild libmysql.a' ],



# grep => [pattern,file] , actions/etc
[ file => $mingw.'include/mysql__h.patch', write => [$mingw.'include/mysql__h.patch',
'*** orig_mysql.h	Fri Jul  6 13:24:56 2007
*** mysql.h_orig        Fri Jan  4 19:35:33 2008
--- mysql.h     Fri Jan  4 16:45:46 2008
***************
*** 45,51 ****
  #include <winsock.h>                          /* For windows */
  #endif
  typedef char my_bool;
! #if (defined(_WIN32) || defined(_WIN64)) && !defined(__WIN__)
  #define __WIN__
  #endif
  #if !defined(__WIN__)
--- 45,51 ----
  #include <winsock.h>                          /* For windows */
  #endif
  typedef char my_bool;
! #if (defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__)) && !defined(__WIN__)
  #define __WIN__
  #endif
  #if !defined(__WIN__)
' ],comment => 'write the patch to the the mysql.h file'],


[ grep => ['\|\| defined\(__MINGW32__\)',$mingw.'include/mysql.h'], shell => ["cd $unixmingw/include","patch -p0 < mysql__h.patch"],comment => 'Apply mysql.h patch file, if not already applied....' ],


# fetch it
[ dir =>     $sources.'pthread', mkdirs => $sources.'pthread' ],
[ archive => $sources.'pthread/libpthread.a',   'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/lib/libpthreadGC2.a',comment => 'libpthread is precompiled, we just download it to the right place ' ],
[ archive => $sources.'pthread/pthreadGC2.dll', 'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/lib/pthreadGC2.dll' ],
[ file    => $sources.'pthread/pthread.dll',       exec => "copy /Y ".$sources."pthread/pthreadGC2.dll ".$sources."pthread/pthread.dll" ],
[ archive => $sources.'pthread/pthread.h',      'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/pthread.h' ],
[ archive => $sources.'pthread/sched.h',        'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/sched.h' ],
[ archive => $sources.'pthread/semaphore.h',    'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/dll-latest/include/semaphore.h' ],
# install it:
[ file => $mingw.'lib/libpthread.a',    exec => "copy /Y ".$dossources."pthread/libpthread.a ".$dosmingw.'lib/libpthread.a',comment => 'install pthread' ],
[ file => $mingw.'bin/pthreadGC2.dll',  exec => "copy /Y ".$dossources."pthread/pthreadGC2.dll ".$dosmingw.'bin/pthreadGC2.dll' ],
[ file => $mingw.'bin/pthread.dll',     exec => "copy /Y ".$dossources."pthread/pthread.dll ".$dosmingw.'bin/pthread.dll' ],
[ file => $mingw.'include/pthread.h',   exec => "copy /Y ".$dossources."pthread/pthread.h ".$dosmingw.'include/pthread.h' ],
[ file => $mingw.'include/sched.h',     exec => "copy /Y ".$dossources."pthread/sched.h ".$dosmingw.'include/sched.h' ],
[ file => $mingw.'include/semaphore.h', exec => "copy /Y ".$dossources."pthread/semaphore.h ".$dosmingw.'include/semaphore.h' ],


## download the MS directX SDK
##  believe it or not, the above exe(dxsdk_november2007.exe) is actually just a zip archive(containing dxsdk_nov2007.exe) with 
## a "read the licence and click OK to unzip", so we just unzip it with a few magic incantations of command lines:
## and the contents (dxsdk_nov2007.exe) is actually just a zip file too, so we unzip it (again) in a subfolder.   finally, the files!
#[ archive => $sources.'dxsdk_november2007.exe','fetch' => 'http://www.microsoft.com/downloads/info.aspx?na=90&p=&SrcDisplayLang=en&SrcCategoryId=&SrcFamilyId=4b78a58a-e672-4b83-a28e-72b5e93bd60a&u=http%3a%2f%2fdownload.microsoft.com%2fdownload%2fb%2fe%2f7%2fbe7ffe34-903c-410b-bdbc-ee6c018df45c%2fdxsdk_november2007.exe' ],
#[ dir => $sources."dxsdk/", mkdirs => $sources.'dxsdk' ],
## command to extract this .zip file to a folder (this outer wrapper is NOT unzip.exe compatible!):
#[ file => $sources."dxsdk/dxsdk_nov2007.exe", exec => $dossources."dxsdk_november2007.exe /C /Q /T:".$dossources."dxsdk" ],
## extract this  - it's a "winzip self extracter" - which IS "unzip.exe compatible"
#[ file => $sources."dxsdk/Include/dsound.h", exec => "cd ".$dossources."dxsdk/ && ".$dossources."unzip/unzip.exe dxsdk_nov2007.exe" ],
## relocate the dxsdk header files to the folder :
## put ddraw.h dinput.h dsound.h into C:\MinGW\Include
#[ file => $mingw.'include/dsound.h', exec => "copy /Y ".$dossources."dxsdk/Include/dsound.h $dosmingw/include/dsound.h" ],
#[ file => $mingw.'include/dinput.h', exec => "copy /Y ".$dossources."dxsdk/Include/dinput.h $dosmingw/include/dinput.h" ],
#[ file => $mingw.'include/ddraw.h', exec => "copy /Y ".$dossources."dxsdk/Include/ddraw.h $dosmingw/include/ddraw.h" ],
## not sure if we need this too, but it doesn't hurt:
#[ file => $mingw.'include/dsetup.h', exec => "copy /Y ".$dossources."dxsdk/Include/dsetup.h $dosmingw/include/dsetup.h" ],

#   ( save bandwidth compare to the above full SDK where they came from:
[ archive => $sources.'DX9SDK_dsound_Include_subset.zip', 'fetch' => 'http://davidbuzz.googlepages.com/DX9SDK_dsound_Include_subset.zip',comment => 'We download just the required Include files for DX9' ], 
[ dir => $sources.'DX9SDK_dsound_Include_subset', extract => $sources.'DX9SDK_dsound_Include_subset.zip' ],
[ file => $mingw.'include/dsound.h', exec => "copy /Y ".$dossources."DX9SDK_dsound_Include_subset/dsound.h ".$dosmingw."include/dsound.h" ],
[ file => $mingw.'include/dinput.h', exec => "copy /Y ".$dossources."DX9SDK_dsound_Include_subset/dinput.h ".$dosmingw."include/dinput.h" ],
[ file => $mingw.'include/ddraw.h', exec =>  "copy /Y ".$dossources."DX9SDK_dsound_Include_subset/ddraw.h ".$dosmingw."include/ddraw.h" ],
[ file => $mingw.'include/dsetup.h', exec => "copy /Y ".$dossources."DX9SDK_dsound_Include_subset/dsetup.h ".$dosmingw."include/dsetup.h" ],



#----------------------------------------
# now we do each of the source library dependancies in turn: download,extract,build/install
# TODO - ( and just prey that they all work?)  These should really be more detailed, and actually check that we got it installed properly.

# Most of these look for a Makefile as a sign that the ./configure was successful (not necessarily true, but it's a start)
# but this requires that the .tar.gz didn't come with a Makefile in it.

[ archive => $sources.'freetype-2.3.5.tar.gz',  fetch => 'http://download.savannah.nongnu.org/releases/freetype/freetype-2.3.5.tar.gz'],
[ dir => $sources.'freetype-2.3.5', extract => $sources.'freetype-2.3.5.tar' ],
# caution... freetype comes with a Makefile in the .tar.gz, so work around it!
[ file => $sources.'freetype-2.3.5/Makefile_', shell => ["cd $unixsources/freetype-2.3.5","./configure --prefix=/mingw","make","make install","touch $unixsources/freetype-2.3.5/Makefile_"] ],


[ archive => $sources.'lame-3.97.tar.gz',  fetch => 'http://'.$sourceforge.'/sourceforge/lame/lame-3.97.tar.gz'],
[ dir => $sources.'lame-3.97', extract => $sources.'lame-3.97.tar' ],
[ file => $sources.'lame-3.97/Makefile', shell => ["cd $unixsources/lame-3.97","./configure --prefix=$unixmingw","make","make install"] ],

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

#

# two older patches
#[ archive => 'qt.patch.gz' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4270/qt.patch.gz'],
#[ archive => 'qt2.patch' , 'fetch' => 'http://tanas.ca/qt2.patch'],
# OR:
# equivalent patch:
[ archive => $sources.'qt.patch' , 'fetch' => 'http://tanas.ca/qt.patch',comment => ' patch the QT sources'],
[ file => $msys.'qt-3.3.x-p8/qt.patch', exec => "copy /Y ".$dossources."qt.patch ".$msys."qt-3.3.x-p8" ],
[ grep => ["\-lws2_32", $msys.'qt-3.3.x-p8/mkspecs/win32-g++/qmake.conf'], shell => ["cd ".$unixmsys."qt-3.3.x-p8/","patch -p1 < qt.patch"] ],


[ file => $msys.'bin/sh_.exe', shell => ["mv ".$unixmsys."bin/sh.exe ".$unixmsys."bin/sh_.exe"],comment => 'rename msys sh.exe out of the way before building QT! ' ] ,

# write a batch script to build the QT sources here:
# the double slashes in the paths here are because this string is in double quotes (the variables need interpolating, so the slashes geet it too) 
[ file => $msys.'qt-3.3.x-p8/qtsetup.bat', write => [$msys.'qt-3.3.x-p8/qtsetup.bat',
'set QTDIR='.$dosmsys.'qt-3.3.x-p8
set MINGW='.$dosmingw.'
set PATH=%QTDIR%\bin;%MINGW%\bin;%PATH%
set QMAKESPEC=win32-g++
cd %QTDIR%
rem rename '.$dosmsys.'bin\sh.exe sh_.exe
configure.bat -thread -plugin-sql-mysql -opengl -no-sql-sqlite
mingw32-make
mingw32-make install
rem rename '.$dosmsys.'bin\sh_.exe sh.exe
']],


[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll', exec => $dosmsys.'qt-3.3.x-p8/qtsetup.bat',comment => 'Execute qtsetup.bat script to actually build QT now! ' ],


#  (back to sh.exe ) now that we are done !
[ file => $msys.'bin/sh.exe', shell => ["mv ".$unixmsys."bin/sh_.exe ".$unixmsys."bin/sh.exe"],comment => 'rename msys sh_.exe back again!' ] ,

#Copy libqt-mt3.dll to libqt-mt.dll  - but always do it(nocheck), or we could be using an old version:
[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt.dll', shell => ["cp ".$unixmsys."qt-3.3.x-p8/lib/libqt-mt3.dll ".$unixmsys."qt-3.3.x-p8/lib/libqt-mt.dll","nocheck"],comment => 'Copy libqt-mt3.dll to libqt-mt.dll' ] ,


#----------------------------------------

# get mythtv sources, if we don't already have them
# download all the files from the web, and save them here:
[ dir => $mythtv.'mythtv', mkdirs => $mythtv.'mythtv' ],


[ file => $mythtv.'svn_', shell => ["rm ".$unixmythtv."using_proxy_cannot_do_SVN.txt; if [ -n '$proxy' ] ; then touch ".$unixmythtv."using_proxy_cannot_do_SVN.txt; fi","nocheck"],comment => 'disable SVN code fetching if we are using a proxy....' ],

# if we dont have the sources at all, get them all from SVN!  (do a checkout, but only if we don't already have the .pro file as a sign of an  earlier checkout)
# this is some nasty in-line batch-script code, but it works.
;
# mythtv,mythplugins,myththemes
foreach my $comp( @components ) {
push @{$expect}, 
[ file => $mythtv.'using_proxy_cannot_do_SVN.txt', exec => ['set PATH='.$dosmsys.'bin;%PATH% && cd '.$dosmythtv.' && IF NOT EXIST '.$dosmythtv.'mythtv/mythtv.pro svn checkout  http://svn.mythtv.org/svn/trunk/'."$comp $comp","nocheck"],comment => 'Get all the mythtv sources from SVN!:'.$comp ];
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

# 
[ file => $mythtv.'make_run.sh', write => [$mythtv.'make_run.sh',
'#!/bin/bash
source '.$unixmythtv.'qt_env.sh
cd '.$unixmythtv.'/mythtv
# keep around just one earlier verion in run_old:
rm -rf run_old
mv run run_old
mkdir run
# copy exes and dlls to the run folder:
find . -name \\*.exe  | xargs -n1 -i__ cp __ ./run/
find . -name \\*.dll  | xargs -n1 -i__ cp __ ./run/  
# mythtv expects the dll to NOT have the 3 in it.
cp '.$unixmsys.'qt-3.3.x-p8/lib/libqt-mt3.dll '.$unixmsys.'qt-3.3.x-p8/lib/libqt-mt.dll
# mythtv needs the qt dlls at runtime:
cp '.$unixmsys.'qt-3.3.x-p8/lib/*.dll '.$unixmythtv.'mythtv/run
# qt mysql connection dll has to exist in a subfolder called sqldrivers:
mkdir '.$unixmythtv.'mythtv/run/sqldrivers
cp '.$unixmsys.'qt-3.3.x-p8/plugins/sqldrivers/libqsqlmysql.dll '.$unixmythtv.'mythtv/run/sqldrivers 
# pthread dlls and mingwm10.dll are copied from here:
cp '.$unixmingw.'bin/*.dll '.$unixmythtv.'mythtv/run
' ],comment => 'script that will copy all the files necessary for running mythtv out of the build folder into the ./run folder'],

#
[ file => $mythtv.'build_myth.sh', write => [$mythtv.'build_myth.sh',
'#!/bin/bash
source '.$unixmythtv.'qt_env.sh
cd '.$unixmythtv.'mythtv
make clean
make distclean
touch '.$unixmythtv.'mythtv/config/config.pro
./configure --prefix=/usr --disable-dbox2 --disable-hdhomerun --disable-dvb --disable-ivtv --disable-iptv --disable-joystick-menu --disable-xvmc-vld --disable-x11 --disable-xvmc --enable-directx --enable-memalign-hack --cpu=k8 --compile-type=debug && make && make install
#make 
#make install
#cd ..
' ],comment => 'write a script to build main mythtv'],

# 
[ file => $mythtv.'build_plugins.sh', write => [$mythtv.'build_plugins.sh',
'#!/bin/bash
cd '.$unixmythtv.'mythplugins
./configure --prefix=/usr --disable-mythgallery --disable-mythmusic --disable-mytharchive --disable-mythbrowser --disable-mythflix --disable-mythgame --disable-mythnews --disable-mythphone --disable-mythzoneminder --disable-mythweb --enable-aac --enable-libvisual --enable-fftw --compile-type=debug && make && make install
#make
#make install
#cd ..
' ],comment => 'write a script to build mythtv plugins'],

# chmod the shell scripts, everytime
[ file => $mythtv.'_' , shell => ["cd $mythtv","chmod 755 *.sh","nocheck"] ],

#----------------------------------------
# now we build mythtv! 
;

# SVN update every time, before patches, unless we are using a proxy
foreach my $comp( @components ) {
push @{$expect}, 
[ file => $mythtv.'using_proxy_cannot_do_SVN.txt', exec => ["cd ".$dosmythtv."$comp && ".$dosmsys."bin/svn.exe update","nocheck"],comment => 'getting SVN updates for:'.$comp ],
}
push @{$expect}, 


# apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN:

#fixed/closed:

#4390 -  stuff
#[ archive => $sources.'videoout_embedding.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4390/videoout_embedding.patch'],
#[ file => $mythtv.'mythtv/videoout_embedding.patch', exec => "copy /Y $sources/videoout_embedding.patch $mythtv/mythtv/" ],
#[ file => $mythtv.'mythtv/videoout_embedding.patch_', shell => ["cd /c/mythtv/mythtv/","patch -p0 < videoout_embedding.patch","touch videoout_embedding.patch_"] ],

# in SVN at 7th Jan 2008
#[ archive => $sources.'setup.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4391/setup.patch'],
#[ file => $mythtv.'mythtv/setup.patch', exec => "copy /Y ".$dossources."setup.patch ".$dosmythtv."mythtv",comment => '4391: - configgroups patch' ],
#[ grep => ['children\[i\] \&\& children\[i\]->isVisible\(\)',$mythtv.'mythtv/libs/libmyth/mythconfiggroups.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < setup.patch"] ],

##

# in SVN at 7th Jan 2008
#[ archive => $sources.'mythwelcome.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4409/mythwelcome.patch'],
#[ file => $mythtv.'mythtv/mythwelcome.patch', exec => "copy /Y ".$dossources."mythwelcome.patch ".$dosmythtv."mythtv",comment => '4409 mythwelcome patch: (MinGW SIGHUP undefined)' ],
#[ grep => ['\#include \"libmyth\/compat\.h\"',$mythtv.'mythtv/programs/mythwelcome/main.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < mythwelcome.patch"] ],

# no longer required as at [15335] - 7th jan 2008
#[ archive => $sources.'themereload_win32.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4411/themereload_win32.patch'],
#[ file => $mythtv.'mythtv/themereload_win32.patch', exec => "copy /Y ".$dossources."themereload_win32.patch ".$dosmythtv."mythtv",comment => '4411 changeset 15290 is incompatible with Win32' ],
#[ grep => ['\#ifndef _WIN32',$mythtv.'mythtv/programs/mythfrontend/main.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < themereload_win32.patch"] ],

# in SVN at 7th Jan 2008
#[ archive => $sources.'dlerr.win.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4422/dlerr.win.patch'],
#[ file => $mythtv.'mythtv/dlerr.win.patch', exec => "copy /Y ".$dossources."dlerr.win.patch ".$dosmythtv."mythtv",comment => '4422 fixes error: "call of overloaded QString(DWORD) is ambiguous" ' ],
#[ grep => ['inline const char \*dlerror(void)',$mythtv.'mythtv/libs/libmyth/compat.h'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < dlerr.win.patch"] ],


[ archive => $sources.'backend.patch.gz' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4392/backend.patch.gz'],
[ file => $mythtv.'mythtv/backend.patch.gz', exec => "copy /Y ".$dossources."backend.patch.gz ".$dosmythtv."mythtv",comment => '4392: - backend connections being accepted patch ' ],
[ grep => ['unsigned\* Indexes = new unsigned\[n\]\;',$mythtv.'mythtv/libs/libmyth/mythsocket.cpp'], shell => ["cd ".$unixmythtv."mythtv/","gunzip -f backend.patch.gz","patch -p0 < backend.patch"] ],


[ archive => $sources.'importicons_windows_2.diff' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/3334/importicons_windows_2.diff'],
[ file => $mythtv.'mythtv/importicons_windows_2.diff', exec => "copy /Y ".$dossources."importicons_windows_2.diff ".$dosmythtv."mythtv",comment => '3334 fixes error with mkdir() unknown. ' ],
[ grep => ['\#include <qdir\.h>',$mythtv.'mythtv/libs/libmythtv/importicons.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < importicons_windows_2.diff"] ],

# next the build process: 
# 
[ file => $mythtv.'no_rebuild_mythtv.txt', shell => ["cd ".$unixmythtv,'./build_myth.sh','touch no_rebuild_mythtv.txt'],comment => 'execute the mythtv build script (we wrote it earlier), unless its already been built once before ( ie the no_rebuild_mythtv.txt file exists! )' ],

#------------------------------------------------------------------------------

;


sub _end {
	
	comment("This verson of the Win32 Build script last was last tested on: $SVNRELEASE");

print << "END";    
#
# SCRIPT TODO/NOTES:  - further notes on this scripts direction....
# ok, how about the test-run process?  
# execute the $mythtv.'make_run.sh' to put all the exe's and dll's in one place
# create a vanilla mysql.txt file
# check that the local mysql server is running, and configure it:
# we should run: 'C:\\Program Files\\MySQL\\MySQL Server 5.0\\bin\\MySQLInstanceConfig.exe'
# check that mythtv/mythtv/mythconverg will access the mysql database.
# 
END
}

#------------------------------------------------------------------------------

# this is the mainloop that itterates over the above definitions and determines what to do:
# cause:
foreach my $dep ( @{$expect} ) { 
    my @dep = @{$dep};

    #print Dumper(\@dep);

    my $causetype = $dep[0];
    my $cause =  $dep[1];
    my $effecttype = $dep[2];
    my $effectparams = $dep[3];
    die "too many parameters in cause->event declaration (@dep)" if defined $dep[4] && $dep[4] ne 'comment';  # four pieces: cause => [blah] , effect => [blah]
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

    my @effectparams;
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
            die "archive -> EFFECT FAILED: $causetype,$cause[0],$effecttype\n";
        }

    }   elsif ( $causetype eq 'dir' ) {
        if ( -d $cause[0] ) {
            print "directory exists: $causetype,$cause[0]\n"; next;
        }
        effect($effecttype,@nocheckeffectparams);
        if ( ! -d $cause[0] && $nocheck == 0) {
            die "dir -> EFFECT FAILED: $causetype,$cause[0],$effecttype\n";
        }

    } elsif ( $causetype eq 'file' ) {
        if ( -f $cause[0] ) {print "file exists: $cause[0]\n"; next;}
        effect($effecttype,@nocheckeffectparams);
        if ( ! -f $cause[0] && $nocheck == 0) {
            die "file -> EFFECT FAILED: $causetype,$cause[0],$effecttype\n";
        }

    } elsif ( $causetype eq 'grep' ) {
        if ( ! _grep($cause[0],$cause[1]) ) { # grep actually needs two parameters on the source side of the action
            effect($effecttype,@nocheckeffectparams);   
        } else {
            print "grep - already matched source file($cause[1]), with pattern ($cause[0]) - no action reqd\n";
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

            $cmd =~ s#/#\\#g; # convert all forward to SINGLE backward slashes
                              # (TODO - might be overkill to do all slashes ?)

            # this next set of regex's is YUCK, but it actually works well,
            # if it breaks, we'll just add another special case like these:

            $cmd =~ s#\s\\Y\s# /Y #ig; # it is overkill, so undo specific case/s
                                       # (eg: the /Y flag to the copy command)
            $cmd =~ s#\s\\C\s# /C #ig; # the /C /Q and /T options
                                       # used to extract the dxsdk
            $cmd =~ s#\s\\Q\s# /Q #ig;
            $cmd =~ s#\s\\T# /T#ig;
            $cmd =~ s#http:\\\\#http://#ig; # dont backslash web addresses
            if ($cmd =~ m/http:/ ) { 
                # next three lines will return any incorrectly backslashed
                # slashes in a URL like: http://blah\blah\blah
                #                     to http://blah/blah/blah as it should be:
                $cmd =~ s#^(.*)(http://.*$)#$2#i;
                my $pre = $1;
                $cmd =~ s#\\#/#g;
                print "pre:$pre\ncmd:$cmd\n";
                $cmd = $pre.$cmd;
            }

            #print `$cmd`;
            print "exec:$cmd\n";
            open F, "$cmd |"  || die "err: $!";
            while (<F>) {
                print;
            }   

        } elsif ($effecttype eq 'shell') {
            shell(@effectparams);

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
}
#------------------------------------------------------------------------------


# execute a sequence of commands in a bash shell.
# we explicitly add the /bin and /mingw/bin to the path
# because at this point they aren't likely to be there
# (cause we are in the process of installing them)
sub shell {
    my @cmds = @_;
    my $cmd = $dosmsys.'bin\bash.exe -c "( export PATH=/bin:/mingw/bin:$PATH;'.join(';',@cmds).')"';
    print "shell:$cmd\n";
    # execute the cmd, and capture the output!  this is a glorified version
    # of "print `$cmd`;" except it doesn't buffer the output if $|=1; is set.
    open F, "$cmd |"  || die "err: $!";
    while (<F>) {
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
    $p =~ s#$msys#/#i;  # remove superflouus msys folders if they are there
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
    print "grep-ing for pattern($pattern) in file($file\n";
    my $fh = IO::File->new("< $file") || die "unable to read file: $file\n";
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
