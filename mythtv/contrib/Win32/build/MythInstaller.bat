@rem = '--*-Perl-*--
@rem
@rem Wrapper for bootstrapping Windows build and install.
@rem Checks for Perl, attempts to download latest win32-packager.pl,
@rem and then executes that with any supplied arguments.
@rem 

@echo off

if "%OS%" == "Windows_NT" setlocal
set PERL5LIB=
set PERLLIB=
set PERL5OPT=
set PERLIO=
set PERL_UNICODE=
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl

:WinNT
if not exist c:\perl\bin\perl.exe goto no_perl
@rem
@rem If we needed any non-standard Perl modules, this is how to install them:
@rem if NOT exist C:\Perl\site\lib\Config\inifiles.pm call c:\perl\bin\ppm.bat install Config-inifiles
@rem if errorlevel 1  echo You do not have Perl in your PATH. 
@rem
perl -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl

:no_perl
echo Could not find Perl in C:\Perl.
echo Please download ActivePerl from http://www.activestate.com/store
echo        install it, and then run this script again.
sleep 1
c:\windows\explorer http://www.activestate.com/store/activeperl/download
goto endofperl

@rem ';
#!c:/perl/bin/perl.exe -w
use strict;
use LWP::UserAgent;
use IO::File; 

my $file = 'win32-packager.pl';
my $url  = "http://svn.mythtv.org/svn/trunk/mythtv/contrib/Win32/build/$file";
my $proxy = ''; # or somehting like: 'http://enter.your.proxy.here:8080';

my $ua = LWP::UserAgent->new;
$ua->proxy(['http', 'ftp'], $proxy);
my $req = HTTP::Request->new(GET => $url);
my $res = $ua->request($req);

if ($res->is_success) {
    my $f = new IO::File "> $file" || die "Can't create $file: $!\n";
    $f->binmode();
    $f->print($res->content);
    $f->close();
}

if ( ! -s $file ) {
    die "Cannot download $url";
}

exec "c:/perl/bin/perl.exe -w $file @ARGV";
__END__
:endofperl
