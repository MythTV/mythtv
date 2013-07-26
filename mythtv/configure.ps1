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
    Write-Host "Launching: $exe $params" -foregroundcolor blue

    $ps = new-object System.Diagnostics.Process

    $ps.StartInfo.Filename               = $exe
    $ps.StartInfo.WorkingDirectory       = $path
    $ps.StartInfo.Arguments              = $params
    $ps.StartInfo.UseShellExecute        = $false
    $ps.start()

    $ps.WaitForExit()

    if ($ps.ExitCode -ne 0) 
    {
        throw "cmake exited with error code $LastExitCode."
    }
}

# ###########################################################################
#
# Check for existance of required Environment and Tools
#
# ###########################################################################

$BuildType = "debug"
$DestDir   = "bin\$BuildType"

Write-Host "--------------------------------------------------------------------------"  -foregroundcolor green
Write-Host "Performing Environmental Checks" -foregroundcolor green
Write-Host "--------------------------------------------------------------------------"  -foregroundcolor green

if (-not (Test-Path Env:\VCINSTALLDIR) )
{
    Write-Host "Error: This script must be run from a Visual Studio Command Prompt."         -ForegroundColor red
    Write-Host "--------------------------------------------------------------------------"  -ForegroundColor red
    exit -1
}

# lets get version of visual studio for cmake.

$VCVerStr = $Env:VCINSTALLDIR -replace ".*(Visual Studio [0-9][0-9]).*", '$1'

Write-Host "Using     : [$VCVerStr]"  -ForegroundColor Cyan
Write-Host "Build Type: [$BuildType]" -ForegroundColor Cyan

Write-Host

# ---------------------------------------------------------------------------


$tools = @{ "nmake"             = "Path";
            "cmake"             = "Path";
            "nasm"              = "Path";
            "pthreadVC2.lib"    = "Build";
            "zlib.lib"          = "Build";
            "mp3lame.lib"       = "Build";
            "tag.lib"           = "Build";
            "libzmq.lib"        = "Build"
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
            $Tools[ $Key ] = Which( $key )

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
 
    if (!(Test-Path platform\win32\msvc\external\lame\output\libmp3lame-static.lib))
    {

        Run-Exe "platform\win32\msvc\external\lame\" `
                "nmake.exe"                          `
                @( "-f Makefile.MSVC", 
                   "CPU=P3", 
                   "ASM=YES", 
                   "MMX=YES", 
                   "libmp3lame-static.lib" )
    }

    # -- Copy lib to shared folder 
    Copy-Item platform\win32\msvc\external\lame\output\libmp3lame-static.lib   $DestDir\mp3lame.lib

    # Due to way it's includeded in source, lame.h must be in main lame directory, so copy it there.

    Copy-Item platform\win32\msvc\external\lame\include\lame.h   platform\win32\msvc\external\lame\
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( "pthreadVC2.lib") -eq "Build")
{
    Write-Host "Building: pthreads" -foregroundcolor cyan

    if (!(Test-Path platform\win32\msvc\external\pthreads.2\pthread*.lib ))
    {
        Run-Exe "platform\win32\msvc\external\pthreads.2\" "nmake.exe" @( "clean", "VC-inlined" )
    }

    # -- Copy lib to shared folder 
    Copy-Item platform\win32\msvc\external\pthreads.2\pthread*.dll   $DestDir
    Copy-Item platform\win32\msvc\external\pthreads.2\pthread*.lib   $DestDir
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( "zlib.lib") -eq "Build")
{
    Write-Host "Building: zlib" -foregroundcolor cyan

    if (!(Test-Path platform\win32\msvc\external\zlib\*.dll ))
    {
        Run-Exe "platform\win32\msvc\external\zlib\" "nmake.exe" @( "-f win32/Makefile.msc" )
    }

    # -- Copy lib to shared folder 
    Copy-Item platform\win32\msvc\external\zlib\*.dll   $DestDir
    Copy-Item platform\win32\msvc\external\zlib\*.lib   $DestDir
}

# ---------------------------------------------------------------------------

if ($tools.Get_Item( 'tag.lib') -eq "Build")
{
    Write-Host "Building: taglib" -foregroundcolor cyan

    if (!(Test-Path platform\win32\msvc\external\taglib\taglib\*.lib ))
    {
        Remove-Item platform\win32\msvc\external\taglib\CMakeCache.txt

        Run-Exe "platform\win32\msvc\external\taglib\" `
                "cmake.exe"                            `
                @("-G ""NMake Makefiles""";
                  "-DZLIB_INCLUDE_DIR=..\zlib";
                  "-DZLIB_LIBRARY=..\zlib\zlib.lib";
                  "-DENABLE_STATIC=1";
                  "-DWITH_MP4=1";
                  "-DCMAKE_BUILD_TYPE=$BuildType" )

        Run-Exe "platform\win32\msvc\external\taglib" `
                "nmake.exe"                           `
                "tag"
    }

    # -- Copy lib to shared folder 
    Copy-Item platform\win32\msvc\external\taglib\taglib\*.lib   $DestDir
}

Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green
Write-Host 
Write-Host "Building MythTV non-Qt External dependencies"                                -ForegroundColor green
Write-Host "--------------------------------------------------------------------------"  -ForegroundColor green

if ($tools.Get_Item( 'libzmq.lib') -eq "Build")
{
    Write-Host "Building: zeromq" -foregroundcolor cyan

    if (!(Test-Path external\zeromq\builds\msvc\debug\*.lib ))
    {
        # -- Upgrade solution to verion of visual studios being used

        Run-Exe "external\zeromq\builds\msvc\" `
                "devenv.exe"                   `
                @( "msvc.sln",
                   "/upgrade" )

        # -- Build Solution 
    
        Run-Exe "external\zeromq\builds\msvc\" `
                "devenv.exe"                   `
                @( "msvc.sln",
                   "/Build $BuildType" )
    }

    # -- Copy dll & lib to shared folder 
    Copy-Item external\zeromq\builds\msvc\debug\*.lib bin\debug
    Copy-Item external\zeromq\lib\*.dll               bin\debug

}

# ###########################################################################

# --------------------------------------------------------------------------
Write-Host "Running qmake on mythtv..." -foregroundcolor green
# --------------------------------------------------------------------------

Write-Host "qmake mythtv.pro" -Foregroundcolor blue

&qmake mythtv.pro

# --------------------------------------------------------------------------
Write-Host 
Write-Host "Configure Completed." -foregroundcolor green
Write-Host "nmake $BuildType " -foregroundcolor Cyan -NoNewline
Write-Host "To build MythTV" 
# --------------------------------------------------------------------------
