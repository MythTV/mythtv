Param(
    [Parameter( Mandatory=$false, ValueFromPipeline=$true )]
    [ValidateSet( "debug", "release" )]
    [String]
    $BuildType = "debug",

    [Parameter( Mandatory=$false, ValueFromPipeline=$true )]
    [ValidateSet( "nmake", "sln")]
    [String]
    $OutputType = "nmake"

)

# ===========================================================================
# -=>TODO:
#
#  * Create Include files:   config.h -> libmythbase/mythconfig.h
#                            libavutil/avconfig.h
#  * check for mysql
#              Qt
#              Visual Studio Version
# ===========================================================================

# helper Functions

Function Which( [string] $path )
{
    $info = Get-Command $path -ErrorAction SilentlyContinue

    if ($info)
    {
        return $info.path
    }

    return $null
}

Function Run-Exe( [string] $path, [string] $exe, $params )
{
    Write-Host "Launching: $path$exe $params" -foregroundcolor blue

    $ps = new-object System.Diagnostics.Process

    $ps.StartInfo.Filename               = $exe
    $ps.StartInfo.WorkingDirectory       = $path
    $ps.StartInfo.Arguments              = $params
    $ps.StartInfo.UseShellExecute        = $false
    $ps.start()

    $bResult = $ps.WaitForExit()

    if ($ps.ExitCode -ne 0)
    {
        throw "cmake exited with error code $LastExitCode."
    }
}

Function FindMSys()
{
    # first check to see if they set an Env variable pointing to the mingw install

    if (Test-Path Env:\MINGW_INSTALLDIR )
    {
        return $Env:MINGW_INSTALLDIR
    }

    # not set, so check registry (only works if an installer was used for a specific appid)

    $MSysPath = (Get-ItemProperty -Path hklm:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\`{AC2C1BDB-1E91-4F94-B99C-E716FE2E9C75`}_is1 -Name InstallLocation).InstallLocation

    $MSysPath += "msys\1.0\"

    if ( Which( $MSysPath + "msys.bat" ))
    {
        return $MSysPath
    }

    return $null
}

# ###########################################################################
#
# Check for existance of required Environment and Tools
#
# ###########################################################################

$basePath  = (Get-Location).Path
$DestDir   = "$basePath\bin\$BuildType"

Write-Host "--------------------------------------------------------------------------"  -foregroundcolor green
Write-Host "Performing Environmental Checks" -foregroundcolor green
Write-Host "--------------------------------------------------------------------------"  -foregroundcolor green

if (-not (Test-Path $DestDir ))
{
    New-Item $DestDir -itemType directory
}

#

if (-not (Test-Path Env:\VCINSTALLDIR ))
{
    Write-Host "Error: This script must be run from a Visual Studio Command Prompt."         -ForegroundColor red
    Write-Host "--------------------------------------------------------------------------"  -ForegroundColor red
    exit -1
}

# lets get version of visual studio for cmake.

$VCVerStr = $Env:VCINSTALLDIR -replace ".*(Visual Studio [0-9][0-9]).*", '$1'

Write-Host "Using      : [$VCVerStr]"   -ForegroundColor Cyan
Write-Host "Build Type : [$BuildType]"  -ForegroundColor Cyan
Write-Host "Output Type: [$OutputType]" -ForegroundColor Cyan

Write-Host

# ---------------------------------------------------------------------------


$tools = @{ "nmake"             = "Path";
            "cmake"             = "Path";
            "nasm"              = "Path";
            "git"               = "Path";
            "msys"              = "Path";    # FFmpeg
            "yasm"              = "Path";    # FFmpeg
            "c99conv"           = "Path";    # FFmpeg
            "c99wrap"           = "Path";    # FFmpeg
            "pthreadVC2.lib"    = "Build";
            "zlib.lib"          = "Build";
            "mp3lame.lib"       = "Build";
            "tag.lib"           = "Build";
            "libzmq.lib"        = "Build";
            "FFmpeg"            = "Build"
          }

$bToolsValid = $TRUE

foreach( $key in $($tools.keys))
{
    $spaces=40 - $key.Length

    Write-Host -NoNewline $("{0}{1,$spaces}" -f  $key, ": ")

    switch ( $Tools[ $Key ] )
    {
        Path
        {
            switch ( $key )
            {
                msys
                {
                    $Tools[ $key ] = FindMSys
                }

                default
                {
                    $Tools[ $Key ] = Which( $key )
                }
            }

            if ($Tools[ $Key ])
            {
                Write-Host "Ok" -ForegroundColor green
            }
            else
            {
                $bToolsValid = $FALSE
                Write-Host "Failed" -ForegroundColor red
            }
        }

        Build
        {
            if ($BuildType -eq "clean")
            {
                Write-Host "Clean" -ForegroundColor Yellow
            }
            else
            {
                if ($key -ne "FFmpeg")
                {
                    if (Test-Path( "$DestDir\$($key)") )
                    {
                        Write-Host "Ok" -ForegroundColor green
                        $Tools[ $Key ] = "Ok"
                    }
                    else
                    {
                        Write-Host "Build" -ForegroundColor Yellow
                    }
                }
                else
                {
                    if (((Test-Path "$DestDir\mythavdevice.lib"  ) -eq $true ) -and `
                        ((Test-Path "$DestDir\mythavfilter.lib"  ) -eq $true ) -and `
                        ((Test-Path "$DestDir\mythavformat.lib"  ) -eq $true ) -and `
                        ((Test-Path "$DestDir\mythavutil.lib"    ) -eq $true ) -and `
                        ((Test-Path "$DestDir\mythswresample.lib") -eq $true ) -and `
                        ((Test-Path "$DestDir\mythswscale.lib"   ) -eq $true ))
                    {
                        Write-Host "Ok" -ForegroundColor green
                        $Tools[ $Key ] = "Ok"
                    }
                    else
                    {
                        Write-Host "Build" -ForegroundColor Yellow
                    }
                }
            }

        }
    }
}

if ( $bToolsValid -eq $FALSE )
{
    Write-Host "--------------------------------------------------------------------------"  -ForegroundColor red
    Write-Host "Required tools Missing.  Please correct and try again."                      -ForegroundColor red
    Write-Host

    exit
}

# ###########################################################################
#
# Build External libraries specific to MSVC
#
# ###########################################################################

Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green
Write-Host
Write-Host "Building MSVC External dependencies"                                         -ForegroundColor green
Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green

if ($tools.Get_Item( "mp3lame.lib") -eq "Build")
{
    Write-Host "Building: mp3lame.lib" -foregroundcolor cyan

    if (!(Test-Path $basePath\platform\win32\msvc\external\lame\output\libmp3lame-static.lib))
    {

        Copy-Item $basePath\platform\win32\msvc\external\lame\configMS.h `
                  $basePath\platform\win32\msvc\external\lame\config.h

        Run-Exe "$basePath\platform\win32\msvc\external\lame\" `
                "nmake.exe"                          `
                @( "-f Makefile.MSVC",
                   "comp=msvc",
                   "asm=no",
                   "libmp3lame-static.lib" )
    }

    # -- Copy lib to shared folder

    Copy-Item $basePath\platform\win32\msvc\external\lame\output\libmp3lame-static.lib `
              $DestDir\mp3lame.lib

    # Due to way it's included in source, lame.h must be in main lame directory, so copy it there.

    Copy-Item $basePath\platform\win32\msvc\external\lame\include\lame.h `
              $basePath\platform\win32\msvc\external\lame\
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( "pthreadVC2.lib") -eq "Build")
{
    Write-Host "Building: pthreads" -foregroundcolor cyan

    if (!(Test-Path $basePath\platform\win32\msvc\external\pthreads.2\pthread*.lib ))
    {
        Run-Exe "$basePath\platform\win32\msvc\external\pthreads.2\" "nmake.exe" @( "clean", "VC-inlined" )
    }

    # -- Copy lib to shared folder
    Copy-Item $basePath\platform\win32\msvc\external\pthreads.2\pthread*.dll   $DestDir
    Copy-Item $basePath\platform\win32\msvc\external\pthreads.2\pthread*.lib   $DestDir
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( "zlib.lib") -eq "Build")
{
    Write-Host "Building: zlib" -foregroundcolor cyan

    if (!(Test-Path $basePath\platform\win32\msvc\external\zlib\*.dll ))
    {
        Run-Exe "$basePath\platform\win32\msvc\external\zlib\" "nmake.exe" @( "-f win32/Makefile.msc" )
    }

    # -- Copy lib to shared folder
    Copy-Item $basePath\platform\win32\msvc\external\zlib\*.dll   $DestDir
    Copy-Item $basePath\platform\win32\msvc\external\zlib\*.lib   $DestDir
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( 'tag.lib') -eq "Build")
{
    Write-Host "Building: taglib" -foregroundcolor cyan

    if (!(Test-Path $basePath\platform\win32\msvc\external\taglib\taglib\*.lib ))
    {
        Remove-Item $basePath\platform\win32\msvc\external\taglib\CMakeCache.txt -ErrorAction SilentlyContinue

        Run-Exe "$basePath\platform\win32\msvc\external\taglib\" `
                "cmake.exe"                            `
                @("-G ""NMake Makefiles""";
                  "-DZLIB_INCLUDE_DIR=..\zlib";
                  "-DZLIB_LIBRARY=..\zlib\zlib.lib";
                  "-DENABLE_STATIC=0";
                  "-DWITH_MP4=1";
                  "-DCMAKE_BUILD_TYPE=$BuildType" )

        Run-Exe "$basePath\platform\win32\msvc\external\taglib" `
                "nmake.exe"                           `
                "tag"
    }

    # -- Copy lib to shared folder
    Copy-Item $basePath\platform\win32\msvc\external\taglib\taglib\*.lib   $DestDir
    Copy-Item $basePath\platform\win32\msvc\external\taglib\taglib\*.dll   $DestDir
}

Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green
Write-Host
Write-Host "Building MythTV non-Qt External dependencies"                                -ForegroundColor green
Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green

if ($tools.Get_Item( 'libzmq.lib') -eq "Build")
{
    Write-Host "Building: zeromq" -foregroundcolor cyan

    if (!(Test-Path $basePath\external\zeromq\builds\msvc\$BuildType\*.lib ))
    {
        # -- Upgrade solution to verion of visual studios being used

        Run-Exe "$basePath\external\zeromq\builds\msvc\" `
                "devenv.exe"                   `
                @( "msvc.sln",
                   "/upgrade" )

        # -- Build Solution

        Run-Exe "$basePath\external\zeromq\builds\msvc\" `
                "devenv.exe"                   `
                @( "msvc.sln",
                   "/Build $BuildType" )
    }

    # -- Copy dll & lib to shared folder
    Copy-Item $basePath\external\zeromq\builds\msvc\$BuildType\*.lib $DestDir
    Copy-Item $basePath\external\zeromq\lib\*.dll                    $DestDir

}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( 'FFmpeg') -eq "Build")
{
    Write-Host "Building: FFmpeg" -foregroundcolor cyan

    $rootPath = "/" + $basePath.replace( '\', '/' ).replace( ':', '/' )
    $MSysPath = ($tools[ "msys" ] + "bin\")

    # create a script to call configure and make in msys environment.

    $scriptFile = "$basePath/_buildFFmpeg.sh"
    $OutPath    = $rootPath + "/bin/$BuildType"

    $FFmegsConfigure = "./configure --toolchain=msvc "        + `
                                   "--enable-shared "         + `
                                   "--bindir=$OutPath "       + `
                                   "--libdir=$OutPath "       + `
                                   "--shlibdir=$OutPath "     + `
                                   "--extra-cflags=-MD "      + `
                                   "--extra-cxxflags=-MD "    + `
                                   "--disable-decoder=mpeg_xvmc"

    Out-File $scriptFile         -Encoding Ascii -InputObject "cd $rootPath/external/FFmpeg"
    Out-File $scriptFile -Append -Encoding Ascii -InputObject "pwd"
    Out-File $scriptFile -Append -Encoding Ascii -InputObject "export SRC_PATH_BARE=$rootPath"
    Out-File $scriptFile -Append -Encoding Ascii -InputObject $FFmegsConfigure
    Out-File $scriptFile -Append -Encoding Ascii -InputObject "make && make install"

    write-host "  Running FFmpeg configure & build."
    write-host "  Using MinGW located at: " -NoNewLine ;  write-host $MSysPath -foregroundcolor cyan

    write-host "  NOTE: There may be no output for a long time... Please be patient." -foregroundcolor yellow

    & $MSysPath\sh.exe --login -c $rootPath/_buildFFmpeg.sh

    Remove-Item $scriptFile -ErrorAction SilentlyContinue

    if (-not (((Test-Path "$DestDir\mythavdevice.lib"  ) -eq $true ) -and `
              ((Test-Path "$DestDir\mythavfilter.lib"  ) -eq $true ) -and `
              ((Test-Path "$DestDir\mythavformat.lib"  ) -eq $true ) -and `
              ((Test-Path "$DestDir\mythavutil.lib"    ) -eq $true ) -and `
              ((Test-Path "$DestDir\mythswresample.lib") -eq $true ) -and `
              ((Test-Path "$DestDir\mythswscale.lib"   ) -eq $true )))
    {
        Write-Host "--------------------------------------------------------------------------"  -ForegroundColor red
        Write-Host "Failed to build FFmpeg."                                                     -ForegroundColor red
        Write-Host

        exit
    }
}


# ###########################################################################

if ($OutputType -eq "nmake")
{
    # -----------------------------------------------------------------------
    # delete external\Makefile since version in Git is for Linux.
    # it will be re-created by qmake using external.pro
    # -----------------------------------------------------------------------

    Remove-Item $basePath\external\Makefile -ErrorAction SilentlyContinue

    # -----------------------------------------------------------------------
    Write-Host "Running qmake on mythtv to generating makefiles..." -foregroundcolor green
    # -----------------------------------------------------------------------

    Write-Host "qmake mythtv.pro" -Foregroundcolor blue

    &qmake mythtv.pro

    # -----------------------------------------------------------------------
    Write-Host
    Write-Host "Configure Completed." -foregroundcolor green
    Write-Host "nmake $BuildType " -foregroundcolor Cyan -NoNewline
    Write-Host "To build MythTV"
    Write-Host
    # -----------------------------------------------------------------------
}
else
{

    # -----------------------------------------------------------------------
    Write-Host "Running qmake on mythtv to generate Visual Studio Solution..." -foregroundcolor green
    # -----------------------------------------------------------------------

    Write-Host "qmake mythtv.pro -tp vc -r" -Foregroundcolor blue

    &qmake mythtv.pro -tp vc -r

    # -----------------------------------------------------------------------
    Write-Host
    Write-Host "Configure Completed." -foregroundcolor green
    Write-Host "You can now open the " -NoNewline
    Write-Host "mythtv.sln" -foregroundcolor Cyan -NoNewline
    Write-Host " file in $VCVerStr"
    Write-Host
    # -----------------------------------------------------------------------
}

