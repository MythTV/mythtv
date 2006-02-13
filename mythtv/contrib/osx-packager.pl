#!/usr/bin/perl

### osx-packager.pl
### Tool for automating frontend builds on Mac OS X.
### Run "osx-packager.pl -man" for full documentation.

use strict;
use Getopt::Long qw(:config auto_abbrev);
use Pod::Usage ();
use Cwd ();

### Configuration settings (stuff that might change more often)

# We try to auto-locate the Subversion client binaries.
# If they are not in your path, you should use the second line.
#
our $svn = `which svn`; chomp $svn;
#our $svn = '/Volumes/Users/nigel/bin/svn';

# This script used to always delete the installed include and lib dirs.
# That probably ensures a safe build, but when rebuilding adds minutes to
# the total build time, and prevents us skipping some parts of a full build
#
our $cleanLibs = 1;

# By default, only the frontend is built (i.e. no backend or transcoding)
#
our $backend = 0;
our $jobtools = 0;

# For faster downloads, change this to a local mirror.
#
our $sourceforge = 'http://internap.dl.sf.net';

# At the moment, there is mythtv plus these two
our @components = ( 'myththemes', 'mythplugins' );

# The OS X programs that we are likely to be interested in.
our @targetsJT = ( 'MythCommFlag',  'MythJobQueue');
our @targetsBE = ( 'MythBackend',   'MythFillDatabase',
                   'MythTranscode', 'MythTV-Setup');

our %depend_order = (
  'mythtv'
  =>  [
        'freetype',
        'lame',
        'mysqlclient',
        'qt-mt',
        'dvdnav'
      ],
  'mythplugins'
  =>  [
        'tiff',
        'exif',
        'dvdcss',
        'dvdread',
#        'cdaudio'
      ],
);

our %depend = (

  'freetype'
  =>
  {
    'url'
    =>  "$sourceforge/sourceforge/freetype/freetype-2.1.10.tar.gz",
  },

  'dvdnav'
  =>
  {
    'url'
    =>  "$sourceforge/sourceforge/dvd/libdvdnav-0.1.10.tar.gz",
  },
  
  'lame'
  =>
  {
    'url'
    =>  "$sourceforge/sourceforge/lame/lame-3.96.1.tar.gz",
    'conf'
    =>  [
          '--disable-frontend',
        ],
  },

#  'cdaudio'
#  =>
#  {
#    'url'
#    =>  "$sourceforge/sourceforge/libcdaudio/libcdaudio-0.99.12.tar.gz"
#  },

  'dvdcss'
  =>
  {
    'url'
    =>  'ftp://ftp.fu-berlin.de/unix/linux/mirrors/gentoo/distfiles/libdvdcss-1.2.8.tar.bz2',
  },

  'dvdread'
  =>
  {
    'url'
    =>  'http://www.dtek.chalmers.se/groups/dvd/dist/libdvdread-0.9.4.tar.gz',
    'conf'
    =>  [
          '--with-libdvdcss',
        ],
  },

 
  'mysqlclient'
  =>
  {
    'url' 
    => 'http://ftp.snt.utwente.nl/pub/software/mysql/Downloads/MySQL-4.1/mysql-4.1.18.tar.gz',
    'conf'
    =>  [
          '--without-debug',
          '--without-docs',
          '--without-man',
          '--without-bench',
          '--without-server',
          '--without-geometry',
          '--without-extra-tools',
        ],
  },
  
  'qt-mt'
  =>
#   {
#     'url'
#     =>  'http://ftp.iasi.roedu.net/mirrors/ftp.trolltech.com/qt/sources/qt-mac-opensource-src-4.0.1.tar.gz',
#     'conf-cmd'
#     =>  'echo yes | ./configure',
#     'conf'
#     =>  [
#           '-prefix', '"$PREFIX"',
#           '-system-zlib',
#           '-fast',
#           '-qt-sql-mysql',
#           '-qt-libpng',
#           '-qt-libjpeg',
#           '-qt-gif',
#           '-platform macx-g++',
#           '-no-tablet',
#           '-I"$PREFIX/include/mysql"',
#           '-L"$PREFIX/lib/mysql"',
#         ],
#     'post-conf'
#     =>  'echo "QMAKE_LFLAGS_SHLIB += -single_module" >> src/qt.pro',
#     'make'
#     =>  [
#           'sub-src'
#         ],
#   },
  {
    'url'
    =>  'ftp://ftp.iasi.roedu.net/mirrors/ftp.trolltech.com/qt/sources/qt-mac-free-3.3.5.tar.gz',
    'conf-cmd'
    =>  'echo yes | ./configure',
    'conf'
    =>  [
          '-prefix', '"$PREFIX"',
          '-thread',
          '-system-zlib',
          '-fast',
          '-qt-sql-mysql',
          '-no-style-cde',
          '-no-style-compact',
          '-no-style-mac',
          '-no-style-motif',
          '-no-style-motifplus',
          '-no-style-platinum',
          '-no-style-sgi',
          '-no-ipv6',
          '-qt-imgfmt-png',
          '-qt-imgfmt-jpeg',
          '-no-imgfmt-mng',
          '-qt-gif',
          '-no-tablet',
          '-I"$PREFIX/include/mysql"',
          '-L"$PREFIX/lib/mysql"',
        ],
    'post-conf'
    =>  'echo "QMAKE_LFLAGS_SHLIB += -single_module" >> src/qt.pro',
    'make'
    =>  [
          'sub-src',
          'qmake-install',
          'moc-install'
        ],
  },
  
  'tiff'
  =>
  {
    'url'
    =>  'ftp://ftp.remotesensing.org/pub/libtiff/old/tiff-3.7.1.tar.gz',
  },
  
  'exif'
  =>
  {
    'url'
    =>  "$sourceforge/sourceforge/libexif/libexif-0.6.12.tar.gz",
    'conf'
    =>  [
          '--disable-nls',
        ],
  },
  
);


=head1 NAME

osx-packager.pl - build OS X binary packages for MythTV

=head1 SYNOPSIS

 osx-packager.pl [options]
 
 Options:
   -help            print the usage message
   -man             print full documentation
   -verbose         print informative messages during the process
   -version <str>   custom version suffix (defaults to "svnYYYYMMDD")
   -noversion       don't use any version (for building release versions)
   -distclean       throw away all intermediate files and exit
   -thirdclean      do a clean rebuild of third party packages
   -thirdskip       don't rebuild the third party packages
   -mythtvskip      don't rebuild/install mythtv
   -pluginskip      don't rebuild/install mythplugins
   -themeskip       don't install the extra themes from myththemes
   -clean           do a clean rebuild of MythTV
   -svnbranch <str> build a specified Subversion branch,   instead of HEAD
   -svnrev <str>    build a specified Subversion revision, instead of HEAD
   -svntag <str>    build a specified release, instead of Subversion HEAD
   -nohead          don't update to HEAD revision of MythTV before building
   -usehdimage      perform build inside of a case-sensitive disk image
   -enable-backend  build the backend server as well as the frontend
   -enable-jobtools build commflag/jobqueue  as well as the frontend
   -plugins <str>   comma-separated list of plugins to include
                      Available plugins:
   mythbrowser mythcontrols mythdvd mythflix mythgallery mythgame
   mythmusic mythnews mythphone mythvideo mythweather

=head1 DESCRIPTION

This script builds a MythTV frontend and all necessary dependencies, along
with plugins as specified, as a standalone binary package for Mac OS X. 

It was designed for building daily CVS (now Subversion) snapshots,
but can also be used to create release builds with the '-svntag' option.

All intermediate files go into an '.osx-packager' directory in the current
working directory. The finished application is named 'MythFrontend.app' and
placed in the current working directory.

=head1 EXAMPLES

Building two snapshots, one with plugins and one without:

  osx-packager.pl -clean -plugins mythvideo,mythweather
  mv MythFrontend.app MythFrontend-plugins.app
  osx-packager.pl -nohead
  mv MythFrontend.app MythFrontend-noplugins.app

Building a 0.17 release build:

  osx-packager.pl -distclean
  osx-packager.pl -svntag release-0-17 -noversion

Building a "fixes" branch:

  osx-packager.pl -distclean
  osx-packager.pl -svnbranch release-0-18-fixes

=head1 CREDITS

Written by Jeremiah Morris (jm@whpress.com)

Special thanks to Nigel Pearson, Jan Ornstedt, Angel Li, and Andre Pang
for help, code, and advice.

Small modifications made by Bas Hulsken (bhulsken@hotmail.com) to allow building current svn, and allow lirc (if installed properly on before running script). The modifications are crappy, and should probably be revised by someone who can actually code in perl. However it works for the moment, and I wanted to share with other mac frontend experimenters!

=cut

# Parse options
our (%OPT);
Getopt::Long::GetOptions(\%OPT,
                         'help|?',
                         'man',
                         'verbose',
                         'version=s',
                         'noversion',
                         'distclean',
                         'thirdclean',
                         'thirdskip',
                         'mythtvskip',
                         'pluginskip',
                         'themeskip',
                         'clean',
                         'svnbranch=s',
                         'svnrev=s',
                         'svntag=s',
                         'nocvs', # This is obsolete, but should stay a while
                         'nohead',
                         'usehdimage',
                         'enable-backend',
                         'enable-jobtools',
                         'plugins=s',
                        ) or Pod::Usage::pod2usage(2);
Pod::Usage::pod2usage(1) if $OPT{'help'};
Pod::Usage::pod2usage('-verbose' => 2) if $OPT{'man'};

if ( $OPT{'enable-backend'} )
{   $backend = 1  }

if ( $OPT{'clean'} )
{   $cleanLibs = 1  }

if ( $OPT{'enable-jobtools'} )
{   $jobtools = 1  }

# Get version string sorted out
if ($OPT{'svntag'} && !$OPT{'version'})
{
  $OPT{'version'} = $OPT{'svntag'};
  $OPT{'version'} =~ s/-r *//;
  $OPT{'version'} =~ s/--revision *//;
}
$OPT{'version'} = '' if $OPT{'noversion'};
unless (defined $OPT{'version'})
{
  my @lt = gmtime(time);
  $OPT{'version'} = sprintf('svn%04d%02d%02d',
                            $lt[5] + 1900,
                            $lt[4] + 1,
                            $lt[3]);
}

# Fold nocvs to nohead
$OPT{'nohead'} = 1 if $OPT{'nocvs'};

# Build our temp directories
our $SCRIPTDIR = Cwd::abs_path(Cwd::getcwd());
if ($SCRIPTDIR =~ /\s/)
{
  &Complain(<<END);
Working directory contains spaces

Error: Your current working path:

   $SCRIPTDIR

contains one or more spaces. This will break the compilation process,
so the script cannot continue. Please re-run this script from a different
directory (such as /tmp).

The application produced will run from any directory, the no-spaces
rule is only for the build process itself.

END
  die;
}

our $WORKDIR = "$SCRIPTDIR/.osx-packager";
mkdir $WORKDIR;

# Do we need to force a case-sensitive disk image?
if (!$OPT{usehdimage} && !CaseSensitiveFilesystem())
{
  Verbose("Forcing -usehdimage due to case-insensitive filesystem");
  $OPT{usehdimage} = 1;
}

if ($OPT{usehdimage})
{
  Verbose("Creating a case-sensitive device for the build");
  MountHDImage();
}

our $PREFIX = "$WORKDIR/build";
mkdir $PREFIX;

our $SRCDIR = "$WORKDIR/src";
mkdir $SRCDIR;

# configure mythplugins, and mythtv, etc
our %conf = (
  'mythplugins'
  =>  [
        '--prefix=' . $PREFIX,
        '--enable-opengl',
        '--disable-mythbrowser',
        '--enable-mythdvd',
        '--enable-vcd',
        '--disable-transcode',
        '--enable-mythgallery',
        '--enable-exif',
        '--enable-new-exif',
        '--disable-mythgame',
        '--disable-mythmusic', 
        '--enable-mythnews',
        '--disable-mythphone',
        '--enable-mythweather',
      ],
  'mythtv'
  =>  [
        '--prefix=' . $PREFIX,
      ],
);

# configure mythplugins, and mythtv, etc
our %makecleanopt = (
  'mythplugins'
  =>  [
        'distclean',
      ],
);

# Source code version.pro needs to call subversion binary
#
use File::Basename;
our $svnpath = dirname $svn;

# Clean the environment
$ENV{'PATH'} = "$PREFIX/bin:/sw/bin:/bin:/usr/bin:$svnpath";
$ENV{'DYLD_LIBRARY_PATH'} = "$PREFIX/lib";
$ENV{'LD_LIBRARY_PATH'} = "/usr/local/lib";
$ENV{'PKG_CONFIG_PATH'} = "$PREFIX/lib/pkgconfig:";
delete $ENV{'CC'};
delete $ENV{'CXX'};
delete $ENV{'CPP'};
delete $ENV{'CXXCPP'};
$ENV{'CFLAGS'} = $ENV{'CXXFLAGS'} = $ENV{'CPPFLAGS'} = "-I$PREFIX/include";
$ENV{'LDFLAGS'} = "-Z -F/System/Library/Frameworks -L/usr/lib -L$PREFIX/lib";
$ENV{'PREFIX'} = $PREFIX;

# set up Qt environment
my $qt_vers = $depend{'qt-mt'}{'url'};
$qt_vers =~ s|^.*/([^/]+)\.tar\.gz$|$1|;
$ENV{'QTDIR'} = "$SRCDIR/$qt_vers";


### Distclean?
if ($OPT{'distclean'})
{
  &Syscall([ '/bin/rm', '-f', '-r', $PREFIX ]);
  &Syscall([ '/bin/rm', '-f', '-r', $SRCDIR ]);
  exit;
}

### Check for app present in target location
our $MFE = "$SCRIPTDIR/MythFrontend.app";
if (-d $MFE)
{
  &Complain(<<END);
$MFE already exists

Error: a MythFrontend application exists where we were planning
to build one. Please move this application away before running
this script.

END
  exit;
}

### Third party packages
my (@build_depends, %seen_depends);
my @comps = ('mythtv', @components);
&Verbose("Including components:", @comps);

foreach my $comp (@comps)
{
  foreach my $dep (@{ $depend_order{$comp} })
  {
    unless (exists $seen_depends{$dep})
    {
      push(@build_depends, $dep);
      $seen_depends{$dep} = 1;
    }
  }
}
foreach my $sw (@build_depends)
{
  # Get info about this package
  my $pkg = $depend{$sw};
  my $url = $pkg->{'url'};
  my $filename = $url;
  $filename =~ s|^.+/([^/]+)$|$1|;
  my $dirname = $filename;
  $dirname =~ s|\.tar\.gz$||;
  $dirname =~ s|\.tar\.bz2$||;

  chdir($SRCDIR);
  
  # Download and decompress
  unless (-e $filename)
  {
    &Verbose("Downloading $sw");
    unless (&Syscall([ '/usr/bin/curl', $url, '>', $filename ],
                     'munge' => 1))
    {
      &Syscall([ '/bin/rm', $filename ]) if (-e $filename);
      die;
    }
  }
  else
  {
    &Verbose("Using previously downloaded $sw");
  }
  
  if ( -d $dirname )
  {
   if ( $OPT{'thirdclean'} )
   {
    &Verbose("Removing previous build of $sw");
    &Syscall([ '/bin/rm', '-f', '-r', $dirname ]) or die;
   }

   if ( $OPT{'thirdskip'} )
   {
    &Verbose("Using previous build of $sw");
    next;
   }

    &Verbose("Using previously unpacked $sw");
  }
  else
  {
    &Verbose("Unpacking $sw");
    if ( substr($filename,-3) eq ".gz" )
    {
      &Syscall([ '/usr/bin/tar', '-xzf', $filename ]) or die;
    }
    elsif ( substr($filename,-4) eq ".bz2" )
    {
      &Syscall([ '/usr/bin/tar', '-xjf', $filename ]) or die;
    }
  }
  
  # Configure
  chdir($dirname);
  unless (-e '.osx-config')
  {
    &Verbose("Configuring $sw");
    my (@configure, $munge);
    
    if ($pkg->{'conf-cmd'})
    {
      push(@configure, $pkg->{'conf-cmd'});
      $munge = 1;
    }
    else
    {
      push(@configure, './configure',
                       '--prefix=$PREFIX',
                       '--disable-static',
                       '--enable-shared');
    }
    if ($pkg->{'conf'})
    {
      push(@configure, @{ $pkg->{'conf'} });
    }
    &Syscall(\@configure, 'interpolate' => 1, 'munge' => $munge) or die;
    if ($pkg->{'post-conf'})
    {
      &Syscall([ $pkg->{'post-conf'} ], 'munge' => 1) or die;
    }
    &Syscall([ '/usr/bin/touch', '.osx-config' ]) or die;
  }
  else
  {
    &Verbose("Using previously configured $sw");
  }
  
  # Build and install
  unless (-e '.osx-built')
  {
    &Verbose("Making $sw");
    my (@make);
    
    push(@make, '/usr/bin/make');
    if ($pkg->{'make'})
    {
      push(@make, @{ $pkg->{'make'} });
    }
    else
    {
      push(@make, 'all', 'install');
    }
    &Syscall(\@make) or die;
    &Syscall([ '/usr/bin/touch', '.osx-built' ]) or die;
  }
  else
  {
    &Verbose("Using previously built $sw");
  }
}


### build MythTV

# Clean any previously installed libraries
if ( $cleanLibs )
{
  if ( $OPT{'mythtvskip'} )
  {
    &Complain("Cannot skip building mythtv src if also cleaning");
    exit;
  }
&Verbose("Cleaning previous installs of MythTV");
my @mythlibs = glob "$PREFIX/lib/libmyth*";
if (scalar @mythlibs)
{
  &Syscall([ '/bin/rm', @mythlibs ]) or die;
}
foreach my $dir ('include', 'lib', 'share')
{
  if (-d "$PREFIX/$dir/mythtv")
  {
    &Syscall([ '/bin/rm', '-f', '-r', "$PREFIX/$dir/mythtv" ]) or die;
  }
}
}

my $svndir = "$SRCDIR/myth-svn";
mkdir $svndir;

# Deal with Subversion branches, revisions and tags:
my $svnrepository = 'http://svn.mythtv.org/svn/';
my @svnrevision   = ();

if ( $OPT{'svnbranch'} )
{
  $svnrepository .= 'branches/' . $OPT{'svnbranch'} . '/';
}
elsif ( $OPT{'svntag'} )
{
  $svnrepository .= 'tags/' . $OPT{'svntag'} . '/';
}
elsif ( $OPT{'svnrev'} )
{
  $svnrepository .= 'trunk/';

  # This arg. could be '1234', '-r 1234', or '--revision 1234'
  # If the user just specified a number, add appropriate flag:
  if ( $OPT{'svnrev'} =~ m/^\d+$/ )
  {
    push @svnrevision, '--revision';
  }

  push @svnrevision, $OPT{'svnrev'};
}
elsif ( ! $OPT{'nohead'} )
{
  # Lookup and use the HEAD revision so we are guaranteed consistent source
  my $cmd = "$svn log $svnrepository --revision HEAD --xml | grep revision";
  &Verbose($cmd);
  my $rev = `$cmd`;
  $rev =~ s/[^[:digit:]]//gs;

  $svnrepository .= 'trunk/';
  @svnrevision = ('--revision', $rev);
}

# Retrieve source
if (! $OPT{'nohead'})
{
  # Empty subdirectory 'config' sometimes causes checkout problems
  &Syscall(['rm', '-fr', $svndir . '/mythtv/config']);
  Verbose("Checking out source code");
  &Syscall([ $svn, 'co', @svnrevision,
            map($svnrepository . $_, @comps), $svndir ]) or die;
}
else
{
  &Syscall("mkdir -p $svndir/mythtv/config")
}

# Deal with user-supplied skip arguments
if ( $OPT{'mythtvskip'} )
{   @comps = grep(!m/mythtv/,      @comps)   }
if ( $OPT{'pluginskip'} )
{   @comps = grep(!m/mythplugins/, @comps)   }
if ( $OPT{'themeskip'} )
{   @comps = grep(!m/myththemes/,  @comps)   }

if ( ! @comps )
{
  &Complain("Nothing to build! Too many ...skip arguments?");
  exit;
}

# Build MythTV and any plugins
foreach my $comp (@comps)
{
  my $compdir = "$svndir/$comp/" ;

  if ( ! -e "$comp.pro" )
  {
    &Complain("$compdir/$comp.pro does not exist.",
              'You must be building really old source?');
    next;
  }

  chdir $compdir;
  
  if ($comp eq 'mythtv')
  {
    # MythTV has an empty subdirectory 'config' that causes problems for me:
    &Syscall('touch config/config.pro');
  }

  if ($OPT{'clean'} && -e 'Makefile')
  {
    &Verbose("Cleaning $comp");
    &Syscall([ '/usr/bin/make', 'distclean' ]) or die;
  }
  else
  {
    # clean the Makefiles, as process requires PREFIX hacking
    &CleanMakefiles();
  }
  
  # configure and make
  if ( $makecleanopt{$comp} && -e 'Makefile' )
  {
    my @makecleancom= '/usr/bin/make';
    push(@makecleancom, @{ $makecleanopt{$comp} }) if $makecleanopt{$comp};
    &Syscall([ @makecleancom ]) or die;
  }
  if (-e 'configure')
  {
    &Verbose("Configuring $comp");
    my @config = './configure';
    push(@config, @{ $conf{$comp} }) if $conf{$comp};
    if ( $comp eq 'mythtv' && $backend )
    {
      push @config, '--enable-backend'
    }
    &Syscall([ @config ]) or die;
  }
  &Verbose("Running qmake for $comp");
  my @qmake_opts = (
    'QMAKE_LFLAGS+=-Wl,-search_paths_first',
    'INCLUDEPATH+="' . $PREFIX . '/include"',
    'LIBS+=-L/usr/lib -L"' . $PREFIX . '/lib"'
   );
  &Syscall([ $PREFIX . '/bin/qmake',
             'PREFIX=../Resources',
             @qmake_opts,
             "$comp.pro" ]) or die;

  if ($comp eq 'mythtv')
  {
    # Remove Nigel's frontend speedup hack
    &DoSpeedupHacks('programs/programs.pro', 'mythfrontend');
    &DoSpeedupHacks('mythtv.pro', 'libs filters programs themes i18n');
  }
  
  &Verbose("Making $comp");
  &Syscall([ '/usr/bin/make' ]) or die;
  # install
  # This requires a change from the compiled-in relative
  # PREFIX to our absolute path of the temp install location.
  &CleanMakefiles();
  &Verbose("Running qmake for $comp install");
  &Syscall([ $PREFIX . '/bin/qmake',
             'PREFIX=' . $PREFIX,
             @qmake_opts,
             "$comp.pro" ]) or die;
  &Verbose("Installing $comp");
  &Syscall([ '/usr/bin/make',
             'install' ]) or die;
}

### Build version string
our $VERS = `find $PREFIX/lib -name 'libmyth-[0-9].[0-9][0-9].[0-9].dylib'`;
chomp $VERS;
$VERS =~ s/^.*\-(.*)\.dylib$/$1/s;
$VERS .= '.' . $OPT{'version'} if $OPT{'version'};

### Create each package.
### Note that this is a bit of a waste of disk space,
### because there are now multiple copies of each library.
my @targets = ('MythFrontend');

if ( $jobtools )
{   push @targets, @targetsJT   }

if ( $backend )
{   push @targets, @targetsBE   }

foreach my $target ( @targets )
{
  my $finalTarget = "$SCRIPTDIR/$target.app";
  my $builtTarget = $target;
  $builtTarget =~ tr/[A-Z]/[a-z]/;

  # Get a fresh copy of the app
  &Verbose("Building self-contained $target");
  &Syscall([ 'rm', '-fr', $finalTarget ]) or die;
  &RecursiveCopy("$PREFIX/bin/$builtTarget.app", $finalTarget);
  
  # write a custom Info.plist
  &GeneratePlist($target, $builtTarget, $finalTarget, $VERS);
  
  # Make frameworks from Myth libraries
  &Verbose("Installing frameworks into $target");
  &PackagedExecutable($finalTarget, $builtTarget);
  
 if ( $target eq "MythFrontend" or $target eq "MythTV-Setup" )
 {
  # Install themes, filters, etc.
  &Verbose("Installing resources into $target");
  mkdir "$finalTarget/Contents/Resources";
  mkdir "$finalTarget/Contents/Resources/lib";
  &RecursiveCopy("$PREFIX/lib/mythtv",
                 "$finalTarget/Contents/Resources/lib");
  mkdir "$finalTarget/Contents/Resources/share";
  &RecursiveCopy("$PREFIX/share/mythtv",
                 "$finalTarget/Contents/Resources/share");
 }
}

if ( $backend )
{
  # The backend gets all the useful binaries it might call:
  foreach my $binary ( 'mythjobqueue', 'mythcommflag', 'mythtranscode' )
  {
    my $SRC  = "$PREFIX/bin/$binary.app/Contents/MacOS/$binary";
    if ( -e $SRC )
    {
      &Syscall([ '/bin/cp', $SRC,
                 "$SCRIPTDIR/MythBackend.app/Contents/MacOS" ]) or die;
      &PackagedExecutable("$SCRIPTDIR/MythBackend.app", $binary);
    }
  }
}

if ( $jobtools )
{
  # JobQueue also gets some binaries it might call:
  my $DEST = "$SCRIPTDIR/MythJobQueue.app/Contents/MacOS";
  my $SRC  = "$PREFIX/bin/mythcommflag.app/Contents/MacOS/mythcommflag";

  &Syscall([ '/bin/cp', $SRC, $DEST ]) or die;
  &PackagedExecutable("$SCRIPTDIR/MythJobQueue.app", 'mythcommflag');

  $SRC  = "$PREFIX/bin/mythtranscode.app/Contents/MacOS/mythtranscode";
  if ( -e $SRC )
  {
      &Syscall([ '/bin/cp', $SRC, $DEST ]) or die;
      &PackagedExecutable("$SCRIPTDIR/MythJobQueue.app", 'mythtranscode');
  }
}

if ($OPT{usehdimage})
{
    Verbose("Dismounting case-sensitive build device");
    UnmountHDImage();
}

&Verbose("Build complete. Self-contained package is at:\n\n    $MFE\n");

### end script
exit 0;


######################################
## RecursiveCopy copies a directory tree, stripping out .svn
## directories and properly managing static libraries.
######################################

sub RecursiveCopy
{
    my ($src, $dst) = @_;

    # First copy absolutely everything
    &Syscall([ '/bin/cp', '-R', "$src", "$dst"]) or die;

    # Then strip out any .svn directories
    my @files = map { chomp $_; $_ } `find $dst -name .svn`;
    if (scalar @files)
    {
        &Syscall([ '/bin/rm', '-f', '-r', @files ]);
    }

    # And make sure any static libraries are properly relocated.
    my @libs = map { chomp $_; $_ } `find $dst -name "lib*.a"`;
    if (scalar @libs)
    {
        &Syscall([ 'ranlib', '-s', @libs ]);
    }
}

######################################
## Given an application package $finalTarget and an executable
## $builtTarget that has been copied into it, PackagedExecutable
## makes sure the package contains all the library dependencies as
## frameworks and that all the paths internal to the executable have
## been adjusted appropriately.
######################################

sub PackagedExecutable($$)
{
    my ($finalTarget, $builtTarget) = @_;

    my $fw_dir = "$finalTarget/Contents/Frameworks";
    mkdir $fw_dir;

    my $dephash
        = &ProcessDependencies("$finalTarget/Contents/MacOS/$builtTarget",
                               glob "$PREFIX/lib/mythtv/*/*.dylib");
    my @deps = values %$dephash;
    while (scalar @deps)
    {
        my $dep = shift @deps;
        next if $dep =~ m/executable_path/;

        my $file = &MakeFramework(&FindLibraryFile($dep), $fw_dir);
        if ( $file )
        {
            my $newhash = &ProcessDependencies($file);
            foreach my $base (keys %$newhash)
            {
                next if exists $dephash->{$base};
                $dephash->{$base} = $newhash->{$base};
                push(@deps, $newhash->{$base});
            }
        }
    }
}


######################################
## MakeFramework copies a dylib into a
## framework bundle.
######################################

sub MakeFramework
{
  my ($dylib, $dest) = @_;
  
  my ($base, $vers) = &BaseVers($dylib);
  $vers .= '.' . $OPT{'version'} if ($OPT{'version'} && $base =~ /myth/);
  my $fw_dir = $dest . '/' . $base . '.framework';

  return '' if ( -e $fw_dir );

  &Verbose("Building $base framework");
  
  &Syscall([ '/bin/mkdir',
             '-p',
             "$fw_dir/Versions/A/Resources" ]) or die;
  &Syscall([ '/bin/cp',
             $dylib,
             "$fw_dir/Versions/A/$base" ]) or die;

  &Syscall([ '/usr/bin/install_name_tool',
             '-id',
             $base,
             "$fw_dir/Versions/A/$base" ]) or die;
  
  symlink('A', "$fw_dir/Versions/Current") or die;
  symlink('Versions/Current/Resources', "$fw_dir/Resources") or die;
  symlink("Versions/A/$base", "$fw_dir/$base") or die;
  
  &Verbose("Writing Info.plist for $base framework");
  my $plist;
  unless (open($plist, '>' . "$fw_dir/Versions/A/Resources/Info.plist"))
  {
    &Complain("Failed to open $base framework's plist for writing");
    die;
  }
  print $plist <<END;
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>$base</string>
  <key>CFBundleIdentifier</key>
  <string>org.mythtv.macx.$base</string>
  <key>CFBundleVersion</key>
  <string>$vers</string>
  <key>CFBundleSignature</key>
  <string>Myth</string>
  <key>CFBundlePackageType</key>
  <string>FMWK</string>
  <key>NSHumanReadableCopyright</key>
  <string>Packaged for the MythTV project, www.mythtv.org</string>
  <key>CFBundleGetInfoString</key>
  <string>lib$base-$vers.dylib, packaged for the MythTV project, www.mythtv.org</string>
</dict>
</plist>
END
  close($plist);
  
  return "$fw_dir/Versions/A/$base";
} # end MakeFramework


######################################
## GeneratePlist .
######################################

sub GeneratePlist
{
  my ($name, $binary, $path, $vers) = @_;
  
  &Verbose("Writing Info.plist for $name");
  my $plist;
  $path .= '/Contents/Info.plist';
  unless (open($plist, ">$path"))
  {
    &Complain("Could not open $path for writing");
    die;
  }
  print $plist <<END;
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key>
  <string>$binary</string>
  <key>CFBundleIconFile</key>
  <string>application.icns</string>
  <key>CFBundleIdentifier</key>
  <string>org.mythtv.macx.$binary</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>$vers</string>
  <key>CFBundleSignature</key>
  <string>Myth</string>
  <key>CFBundleVersion</key>
  <string>$vers</string>
  <key>NSAppleScriptEnabled</key>
  <string>NO</string>
  <key>CFBundleGetInfoString</key>
  <string>$vers, MythTV project, www.mythtv.org</string>
  <key>CFBundleName</key>
  <string>$name</string>
  <key>NSHumanReadableCopyright</key>
  <string>MythTV project, www.mythtv.org</string>
</dict>
</plist>
END
  close($plist);
  
  $path =~ s/Info\.plist$/PkgInfo/;
  unless (open($plist, ">$path"))
  {
    &Complain("Could not open $path for writing");
    die;
  }
  print $plist <<END;
APPLMyth
END
  close($plist);
}

######################################
## FindLibraryFile locates a dylib.
######################################

sub FindLibraryFile
{
  my ($dylib) = @_;
  
  return Cwd::abs_path($dylib) if (-e $dylib);
  return Cwd::abs_path("$PREFIX/lib/$dylib") if (-e "$PREFIX/lib/$dylib");
  return Cwd::abs_path("$ENV{'QTDIR'}/lib/$dylib") if (-e "$ENV{'QTDIR'}/lib/$dylib");
  my @sublibs = glob("$PREFIX/lib/*/$dylib");
  return Cwd::abs_path($sublibs[0]) if (scalar @sublibs);
  &Complain("Could not find $dylib");
  die;
} # end FindLibraryFile


######################################
## ProcessDependencies catalogs and
## rewrites dependencies that will be
## packaged into our app bundle.
######################################

sub ProcessDependencies
{
  my (%depfiles);
  
  foreach my $file (@_)
  {
    &Verbose("Processing shared library dependencies for $file");
    my ($filebase) = &BaseVers($file);
    
    my $cmd = "otool -L $file";
    &Verbose($cmd);
    my @deps = `$cmd`;
    shift @deps;  # first line just repeats filename
    &Verbose("Dependencies for $file = @deps");
    foreach my $dep (@deps)
    {
      chomp $dep;

      # otool returns lines like:
      #    libblah-7.dylib   (compatibility version 7, current version 7)
      # but we only want the file part
      $dep =~ s/\s+(.*) \(.*\)$/$1/;

      # Paths like /usr/lib/libstdc++ contain quantifiers that must be escaped
      $dep =~ s/([+*?])/\\$1/;
      
      # otool sometimes lists the framework as depending on itself
      next if ($file =~ m,/Versions/A/$dep,);

      # Any dependency which is already package relative can be ignored
      next if $dep =~ m/\@executable_path/;

      # skip system library locations
      next if ($dep =~ m|^/System|  ||
               $dep =~ m|^/usr/lib|);
      
      my ($base) = &BaseVers($dep);
      $depfiles{$base} = $dep;
      
      # Change references while we're here
      &Syscall([ '/usr/bin/install_name_tool',
                 '-change',
                 $dep,
                 "\@executable_path/../Frameworks/$base.framework/$base",
                 $file ]) or die;
    }
  }
  return \%depfiles;
} # end ProcessDependencies


######################################
## BaseVers splits up a dylib file
## name for framework naming.
######################################

sub BaseVers
{
  my ($filename) = @_;
  
  if ($filename =~ m|^(?:.*/)?lib(.*)\-(\d.*)\.dylib$|)
  {
    return ($1, $2);
  }
  elsif ($filename =~ m|^(?:.*/)?lib(.*?)\.(\d.*)\.dylib$|)
  {
    return ($1, $2);
  }
  elsif ($filename =~ m|^(?:.*/)?lib(.*?)\.dylib$|)
  {
    return ($1, undef);
  }
  
  &Verbose("Not a library file: $filename");
  return $filename;
} # end BaseVers
  

######################################
## CleanMakefiles removes every
## Makefile from our MythTV build.
## Necessary when we change the
## PREFIX variable.
######################################

sub CleanMakefiles
{
  &Verbose("Cleaning MythTV makefiles");
  my @files = map { chomp $_; $_ } `find . -name Makefile`;
  if (scalar @files)
  {
    &Syscall([ '/bin/rm', @files ]) or die;
  }
} # end CleanMakefiles


######################################
## Syscall wrappers the Perl "system"
## routine with verbosity and error
## checking.
######################################

sub Syscall
{
  my ($arglist, %opts) = @_;
  
  unless (ref $arglist)
  {
    $arglist = [ $arglist ];
  }
  if ($opts{'interpolate'})
  {
    my @args;
    foreach my $arg (@$arglist)
    {
      $arg =~ s/\$PREFIX/$PREFIX/ge;
      push(@args, $arg);
    }
    $arglist = \@args;
  } 
  if ($opts{'munge'})
  {
    $arglist = [ join(' ', @$arglist) ];
  }
  # clean out any null arguments
  $arglist = [ map $_, @$arglist ];
  &Verbose(@$arglist);
  my $ret = system(@$arglist);
  if ($ret)
  {
    &Complain('Failed system call: "', @$arglist,
              '" with error code', $ret >> 8);
  }
  return ($ret == 0);
} # end Syscall


######################################
## Verbose prints messages in verbose
## mode.
######################################

sub Verbose
{
  print STDERR '[osx-pkg] ' . join(' ', @_) . "\n"
      if $OPT{'verbose'};
} # end Verbose


######################################
## Complain prints messages in any
## verbosity mode.
######################################

sub Complain
{
  print STDERR '[osx-pkg] ' . join(' ', @_) . "\n";
} # end Complain


######################################
## Manage usehdimage disk image
######################################

sub MountHDImage
{
    if (!HDImageDevice())
    {
        if (! -e "$SCRIPTDIR/.osx-packager.dmg")
        {
            Syscall(['hdiutil', 'create', '-size', '1200m',
                     "$SCRIPTDIR/.osx-packager.dmg", '-volname',
                     'MythTvPackagerHDImage', '-fs', 'UFS', '-quiet']);
        }

        Syscall(['hdiutil', 'mount', "$SCRIPTDIR/.osx-packager.dmg",
                 '-mountpoint', $WORKDIR, '-quiet']);
    }

    # configure defaults to /tmp and OSX barfs when mv crosses
    # filesystems so tell configure to put temp files on the image

    $ENV{TMPDIR} = $WORKDIR . "/tmp";
    mkdir $ENV{TMPDIR};
}

sub UnmountHDImage
{
    my $device = HDImageDevice();
    if ($device)
    {
        Syscall(['hdiutil', 'detach', $device, '-force']);
    }
}

sub HDImageDevice
{
    my @dev = split ' ', `/sbin/mount | grep $WORKDIR`;
    $dev[0];
}

sub CaseSensitiveFilesystem
{
  my $funky = $SCRIPTDIR . "/.osx-packager.FunkyStuff";
  my $unfunky = substr($funky, 0, -10) . "FUNKySTuFF";

  unlink $funky if -e $funky;
  `touch $funky`;
  my $sensitivity = ! -e $unfunky;
  unlink $funky;

  return $sensitivity;
}


######################################
## Remove or add Nigel's speedup hacks
######################################

sub DoSpeedupHacks($$)
{
  my ($file, $subdirs) = @_;

  &Verbose("Removing Nigel's hacks from file $file");

  open(IN,  $file)         or die;
  open(OUT, ">$file.orig") or die;
  while ( <IN> )
  {
    if ( m/^# Nigel/ )  # Skip
    {  last  }
    print OUT;
  }
  if ( ! $backend && ! $jobtools )
  {
    # Nigel's hack to speedup building
    print OUT "# Nigel\'s speedup hack:\n";
    print OUT "SUBDIRS = $subdirs\n";
  }
  close IN; close OUT;
  rename("$file.orig", $file);
}

### end file
1;
