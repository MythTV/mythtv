#!/usr/bin/perl

### osx-packager.pl
### Tool for automating frontend builds on Mac OS X.
### Run "osx-packager.pl -man" for full documentation.

use strict;
use Getopt::Long qw(:config auto_abbrev);
use Pod::Usage ();
use Cwd ();

### Configuration settings (stuff that might change more often)

#our @components = ( 'mythweather', 'mythnews', 'mythgame',
#                    'mythgallery', 'mythvideo', 'mythdvd' );

# added by Bas to download themes and all plugins
our @components = ( 'myththemes', 'mythplugins' );


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
    =>  'http://unc.dl.sf.net/sourceforge/freetype/freetype-2.1.9.tar.gz',
  },

  'dvdnav'
  =>
  {
    'url'
    =>  'http://umn.dl.sourceforge.net/sourceforge/dvd/libdvdnav-0.1.10.tar.gz',
  },
  
  'lame'
  =>
  {
    'url'
    =>  'http://unc.dl.sf.net/sourceforge/lame/lame-3.96.1.tar.gz',
    'conf'
    =>  [
          '--disable-frontend',
        ],
  },

#  'cdaudio'
#  =>
#  {
#    'url'
#    =>  'http://easynews.dl.sourceforge.net/sourceforge/libcdaudio/libcdaudio-0.99.12.tar.gz'
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
    => 'http://ftp.snt.utwente.nl/pub/software/mysql/Downloads/MySQL-4.1/mysql-4.1.12.tar.gz',
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
  {
    'url'
    =>  'ftp://ftp.iasi.roedu.net/mirrors/ftp.trolltech.com/qt/sources/qt-mac-free-3.3.4.tar.gz',
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
    =>  'http://unc.dl.sf.net/sourceforge/libexif/libexif-0.6.12.tar.gz',
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
   -version <str>   custom version suffix (defaults to "cvsYYYYMMDD")
   -noversion       don't use any version (for building release versions)
   -distclean       throw away all intermediate files and exit
   -thirdclean      do a clean rebuild of third party packages
   -clean           do a clean rebuild of MythTV
   -cvstag <str>    build specified CVS release, instead of HEAD
   -nocvs           don't update MythTV CVS before building
   -plugins <str>   comma-separated list of plugins to include
                      Available plugins:
                          mythvideo mythweather mythgallery mythgame
                          mythdvd mythnews ALL

=head1 DESCRIPTION

This script builds a MythTV frontend and all necessary dependencies, along
with plugins as specified, as a standalone binary package for Mac OS X. 

It was designed for building daily CVS snapshots, but can also be used to
create release builds with the '-cvstag' option.

All intermediate files go into an '.osx-packager' directory in the current
working directory. The finished application is named 'MythFrontend.app' and
placed in the current working directory.

=head1 EXAMPLES

Building two snapshots, one with plugins and one without:

  osx-packager.pl -clean -plugins mythvideo,mythweather
  mv MythFrontend.app MythFrontend-plugins.app
  osx-packager.pl -nocvs
  mv MythFrontend.app MythFrontend-noplugins.app

Building a 0.17 release build:

  osx-packager.pl -distclean
  osx-packager.pl -cvstag release-0-17 -noversion

=head1 CREDITS

Written by Jeremiah Morris (jm@whpress.com)

Special thanks to Nigel Pearson, Jan Ornstedt, Angel Li, and Andre Pang
for help, code, and advice.

Small modifications made by Bas Hulsken (bhulsken@hotmail.com) to allow building current cvs, and allow lirc (if installed properly on before running script). The modifications are crappy, and should probably be revised by someone who can actually code in perl. However it works for the moment, and I wanted to share with other mac frontend experimenters!

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
                         'clean',
                         'cvstag=s',
                         'nocvs',
                         'plugins=s',
                        ) or Pod::Usage::pod2usage(2);
Pod::Usage::pod2usage(1) if $OPT{'help'};
Pod::Usage::pod2usage('-verbose' => 2) if $OPT{'man'};

# Get version string sorted out
$OPT{'version'} = $OPT{'cvstag'} if ($OPT{'cvstag'} && !$OPT{'version'});
$OPT{'version'} = '' if $OPT{'noversion'};
unless (defined $OPT{'version'})
{
  my @lt = gmtime(time);
  $OPT{'version'} = sprintf('cvs%04d%02d%02d',
                            $lt[5] + 1900,
                            $lt[4] + 1,
                            $lt[3]);
}
$OPT{'cvstag'} = 'HEAD' unless $OPT{'cvstag'};

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

mkdir "$SCRIPTDIR/.osx-packager";

our $PREFIX = "$SCRIPTDIR/.osx-packager/build";
mkdir $PREFIX;

our $SRCDIR = "$SCRIPTDIR/.osx-packager/src";
mkdir $SRCDIR;

# configure mythplugins, and mythtv, etc
our %conf = (
  'mythplugins'
  =>  [
        '--enable-opengl',
        '--disable-mythbrowser',
        '--enable-mythdvd',
        '--enable-vcd',
        '--disable-transcode',
        '--enable-mythgallery',
        '--enable-exif',
        '--disable-mythgame',
        '--disable-mythmusic', 
        '--disable-mythnews',
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

# Clean the environment
$ENV{'PATH'} = "$PREFIX/bin:/bin:/usr/bin";
$ENV{'DYLD_LIBRARY_PATH'} = "$PREFIX/lib";
$ENV{'LD_LIBRARY_PATH'} = "/usr/local/lib";
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
my @comps = ('mythtv');
#if ($OPT{'plugins'} eq 'ALL')
#{
#  push(@comps, @components);
#}
#elsif ($OPT{'plugins'})
#{
#  push(@comps, split(',', $OPT{'plugins'}));
#}
# changed by Bas, always build working plugins!
push(@comps, @components);
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
  
  if ($OPT{'thirdclean'} && -d $dirname)
  {
    &Verbose("Removing previous build of $sw");
    &Syscall([ '/bin/rm', '-f', '-r', $dirname ]) or die;
  }
  
  unless (-d $dirname)
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
  else
  {
    &Verbose("Using previously unpacked $sw");
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

# prepare CVS
my $cvsdir = "$SRCDIR/myth-cvs";
mkdir $cvsdir;

unless (-e "$cvsdir/.cvspass")
{
  &Verbose("Creating .cvspass file");
  # Create .cvspass file to bypass manual login
  my $passfile;
  unless (open($passfile, ">$cvsdir/.cvspass"))
  {
    &Complain("Failed to open .cvspass for writing");
    die;
  }
  print $passfile <<END;
:pserver:mythtv\@cvs.mythtv.org:/var/lib/mythcvs A%a,c,<
END
  close $passfile;
}
$ENV{'CVS_PASSFILE'} = "$cvsdir/.cvspass";

# Clean any previously installed libraries
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

# Build MythTV and any plugins
foreach my $comp (@comps)
{
  my $compdir = "$cvsdir/$comp/" ;
  my $svn = '/sw/bin/svn';

  # Get CVS
  unless (-d $compdir)
  {
    &Verbose("Checking out $comp");
    chdir $cvsdir;

    &Syscall([ $svn, 'co',
               'http://svn.mythtv.org/svn/trunk/' . $comp, $compdir]) or die;
  }
  elsif (!$OPT{'nocvs'})
  {
    &Verbose("Updating $comp CVS");
    chdir $compdir;
#	print $compdir;
    &Syscall([ $svn, 'update']) or die;
  }
  chdir $compdir;
  
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

### Assemble application package
if (1)
{
  # Get a fresh copy of the app
  &Verbose("Building self-contained MythFrontend");
  &Syscall([ '/bin/cp', '-R',
             "$PREFIX/bin/mythfrontend.app",
             $MFE ]) or die;
  
  # write a custom Info.plist
  &FrontendPlist($MFE, $VERS);
  
  # Make frameworks from Myth libraries
  &Verbose("Installing frameworks into MythFrontend");
  my $fw_dir = "$MFE/Contents/Frameworks";
  mkdir($fw_dir);
  my $dephash = &ProcessDependencies("$MFE/Contents/MacOS/mythfrontend",
                                     glob("$PREFIX/lib/mythtv/*/*.dylib"));
  my @deps = values %$dephash;
  while (scalar @deps)
  {
    my $file = &MakeFramework(&FindLibraryFile(shift @deps), $fw_dir);
    my $newhash = &ProcessDependencies($file);
    foreach my $base (keys %$newhash)
    {
      next if exists $dephash->{$base};
      $dephash->{$base} = $newhash->{$base};
      push(@deps, $newhash->{$base});
    }
  }
  
  # Install themes, filters, etc.
  &Verbose("Installing resources into MythFrontend");
  mkdir "$MFE/Contents/Resources";
  mkdir "$MFE/Contents/Resources/lib";
  &Syscall([ '/bin/cp', '-R',
             "$PREFIX/lib/mythtv",
             "$MFE/Contents/Resources/lib" ]) or die;
  mkdir "$MFE/Contents/Resources/share";
  &Syscall([ '/bin/cp', '-R',
             "$PREFIX/share/mythtv",
             "$MFE/Contents/Resources/share" ]) or die;
}

&Verbose("Build complete. Self-contained package is at:\n\n    $MFE\n");

### end script
exit 0;



######################################
## MakeFramework copies a dylib into a
## framework bundle.
######################################

sub MakeFramework
{
  my ($dylib, $dest) = @_;
  
  my ($base, $vers) = &BaseVers($dylib);
  $vers .= '.' . $OPT{'version'} if ($OPT{'version'} && $base =~ /myth/);
  &Verbose("Building $base framework");
  
  my $fw_dir = $dest . '/' . $base . '.framework';
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
## FrontendPlist .
######################################

sub FrontendPlist
{
  my ($path, $vers) = @_;
  
  &Verbose("Writing Info.plist for MythFrontend");
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
  <string>mythfrontend</string>
  <key>CFBundleIconFile</key>
  <string>application.icns</string>
  <key>CFBundleIdentifier</key>
  <string>org.mythtv.macx.mythfrontend</string>
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
  <string>MythFrontend</string>
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
} # end FrontendPlist

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
    
    my @deps = `otool -L $file`;
    shift @deps;  # first line just repeats filename
    foreach my $dep (@deps)
    {
      chomp $dep;
      $dep =~ s/\s+(.*) \(.*\)$/$1/;
      
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
  
  &Verbose("Could not parse library file name $filename");
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


### end file
1;
