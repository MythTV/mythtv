#!/usr/bin/perl -w
# ============================================================================
# = NAME
# osx-bundler.pl - General purpose application bundling utility
#
# = USAGE
my $usage = '
osx-bundler.pl executable [lib-dir] [lib-dir...]
osx-bundler.pl target.app [lib-dir] [lib-dir...]
osx-bundler.pl target1.app/Contents/MacOS/target2 [lib-dir...]
osx-bundler.pl target1.app/Contents/Resources/lib/extra.dylib [lib-dir...]
';
# The first form builds a new bundle, executable.app
# The second form checks/adds Frameworks in an existing bundle
# The third form checks/updates/adds Frameworks
# for an extra executable in an existing bundle,
# and the fourth form does the same for an extra library.
#
# = DESCRIPTION
# On OS X, make usually just gives you an executable.
# Some toolsets (e.g. TrollTech's Qt) even build a .app bundle.
# But, I couldn't find anything to build a bundle ready to package and
# ship to the customer as a fully standalone application, so here is one.
#
# Basically, this script:
# 1) uses otool to determine any shared libraries that the executable calls
#    (and any that the shared libs call)
# 2) copies them into the bundle's Framework directory
# 3) uses install_name_tool to update the library load paths
#
# = KNOWN BUGS
# Doesn't do anything about Universal Binaries yet
#
# = TO DO
# Add more arguments to allow the user to specify
# .pinfo fields like CFBundleIdentifier, CFBundleSignature,
# NSHumanReadableCopyright, CFBundleGetInfoString, et c.
#
# = REVISION
# $Id$
#
# = AUTHORS
# Nigel Pearson. Based on osx-packager by Jeremiah Morris,
# with improvements by Geoffrey Kruse and David Abrahams
# ============================================================================

use strict;
use Cwd;
use File::Basename;
use Getopt::Long;

sub usage($)
{
    print "Usage:$usage";
    exit @_;
}

if ( $#ARGV < 0 )
{
    print "No application or executable provided\n";
    usage(-1);
}

# ============================================================================

my $verbose = 0;
my $Id = '$Id$';  # Version of this script. From version control system
my $binary;
my $binbase;      # $binary without any directory path
my $bundle;
my @libdirs;
my $target;  # Full path to the binary under $bundle

# Process arguments:

Getopt::Long::GetOptions('verbose' => \$verbose) or usage(-1);

$binary  = shift @ARGV;
@libdirs = @ARGV;

# ============================================================================

if ( $binary =~ m/(.*)\.app$/ )
{
    $bundle = $binary;

    # executable name, which in blah.app is usually blah
    $binbase = basename($1);
    $target = "$bundle/Contents/MacOS/$binbase";

    if ( ! -e $target )
    {
        &Complain("Couldn't locate $target");
        exit -2;
    }
}
elsif ( $binary !~ m/\.app/ )  # No .app means second form (binary executable)
{
    if ( ! -e $binary )
    {
        &Complain("Couldn't locate $binary");
        exit -3;
    }

    $bundle = "$binary.app";

    $binbase = basename($binary);
    $target = "$bundle/Contents/MacOS/$binbase";

    mkdir $bundle || die;
    mkdir "$bundle/Contents";
    mkdir "$bundle/Contents/MacOS";
    &Syscall([ '/bin/cp', '-p', $binary, $target ]) or die;

    # write a custom Info.plist
    &GeneratePlist($binary, $binbase, $bundle, '1.0');
}
elsif ( $binary =~ m/\.app/ )  # Third/fourth form (exe/lib in existing bundle)
{
    $target = $binary;

    if ( ! -e $target )
    {
        &Complain("Couldn't locate $target");
        exit -4;
    }

    $bundle = $target;
    $bundle =~ s/\.app.*/.app/;
}


&Verbose("Installing frameworks into $target");
&PackagedExecutable($bundle, $target, @libdirs);
exit 0;


######################################
## Given an application package $bundle and an executable
## $target that has been copied into it, PackagedExecutable
## makes sure the package contains all the library dependencies as
## frameworks and that all the paths internal to the executable have
## been adjusted appropriately.
######################################

sub PackagedExecutable($$@)
{
    my ($bundle, $target, @libdirs) = @_;

    my $fw_dir = "$bundle/Contents/Frameworks";
    mkdir $fw_dir;
    my $dephash = &ProcessDependencies($target);
    my @deps = values %$dephash;
    while (scalar @deps)
    {
        my $dep = shift @deps;
        next if $dep =~ m/executable_path/;

        $dep = &FindLibraryFile($dep, $bundle, @libdirs);
        my $file = &MakeFramework($dep, $fw_dir);
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
    my $fw_dir = $dest . '/' . $base . '.framework';

    return '' if ( -e $fw_dir );

    &Verbose("Building $base framework");

    &Syscall([ '/bin/mkdir', '-p',
               "$fw_dir/Versions/A/Resources" ]) or die;
    &Syscall([ '/bin/cp', $dylib,
               "$fw_dir/Versions/A/$base" ]) or die;

    &Syscall([ '/usr/bin/install_name_tool',
               '-id', $base, "$fw_dir/Versions/A/$base" ]) or die;

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
  <string>org.osx-bundler.$base</string>
  <key>CFBundleVersion</key>
  <string>$vers</string>
  <key>CFBundleSignature</key>
  <string>osx-bundler</string>
  <key>CFBundlePackageType</key>
  <string>FMWK</string>
  <key>NSHumanReadableCopyright</key>
  <string>Packaged by $Id</string>
  <key>CFBundleGetInfoString</key>
  <string>lib$base-$vers.dylib, packaged by $Id</string>
</dict>
</plist>
END
    close($plist);

    return "$fw_dir/Versions/A/$base";
}


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
  <string>org.osx-bundler.$binary</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>$vers</string>
  <key>CFBundleSignature</key>
  <string>osx-bundler</string>
  <key>CFBundleVersion</key>
  <string>$vers</string>
  <key>NSAppleScriptEnabled</key>
  <string>NO</string>
  <key>CFBundleGetInfoString</key>
  <string>$vers, $Id</string>
  <key>CFBundleName</key>
  <string>$binary</string>
  <key>NSHumanReadableCopyright</key>
  <string>Packaged by $Id</string>
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

sub FindLibraryFile($@)
{
    my ($dylib, $bundle, @libdirs) = @_;
    my $path;

    return Cwd::abs_path($dylib) if (-e $dylib);

    # 

    foreach my $dir ( @libdirs )
    {
        $path = "$dir/$dylib";
        if ( -e $path ) { return Cwd::abs_path($path) }
    }
    &Complain("Could not find $dylib");
    die;
}


######################################
## ProcessDependencies catalogs and
## rewrites dependencies that will be
## packaged into our app bundle.
######################################

sub ProcessDependencies(@)
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
        &Verbose("Dependencies for $file =\n @deps");
        foreach my $dep (@deps)
        {
            chomp $dep;

            # otool returns lines like:
            #    libblah-7.dylib   (compatibility version 7, current version 7)
            # but we only want the file part
            $dep =~ s/\s+(.*) \(.*\)$/$1/;

            # Paths like /usr/lib/libstdc++ contain chars that must be escaped
            $dep =~ s/([+*?])/\\$1/;

            # otool sometimes lists the framework as depending on itself
            next if ($file =~ m,/Versions/A/$dep,);

            # Any dependency which is already package relative can be ignored
            next if $dep =~ m/\@executable_path/;

            # skip system library locations
            next if ($dep =~ m|^/System|  ||
                     $dep =~ m|^/usr/lib|);

            my ($base) = &BaseVers($dep);

            # Only add this dependency if needed. This assumes that 
            # we aren't mixing versions of the same library name
            if ( ! -e "$bundle/Contents/Frameworks/$base.framework/$base" )
            {   $depfiles{$base} = $dep   }

            &Syscall([ '/usr/bin/install_name_tool', '-change', $dep,
                       "\@executable_path/../Frameworks/$base.framework/$base",
                       $file ]) or die;
        }
    }
    return \%depfiles;
}


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
}


# ============================================================================
# Syscall() wraps the Perl "system" routine with verbosity and error checking
# ============================================================================

sub Syscall
{
    my ($arglist, %opts) = @_;

    unless (ref $arglist)
    {
        $arglist = [ $arglist ];
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
}


sub Verbose
{
    if ( $verbose ) { print STDERR 'osx-bundler: ' . join(' ', @_) . "\n" }
}

sub Complain
{
    print STDERR 'osx-bundler: ' . join(' ', @_) . "\n";
}

# ============================================================================

1;
