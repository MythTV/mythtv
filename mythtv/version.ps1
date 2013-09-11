Param( [Parameter( Mandatory=$true, ValueFromPipeline=$true )]
       [String]
       $RootDir )

# generates libs\libmythbase\version.h file 

$SourceVersion = git describe --dirty | Out-String
$SourceVersion = $SourceVersion.SubString( 0, $SourceVersion.length - 2)

$branch_git = git branch --no-color | Out-String -Stream
[string]$branch  = ($branch_git -match '\*')
$branch          = $branch.TrimStart("* ")

Write-Host -NoNewline "Source Version : "; Write-Host $SourceVersion -foregroundColor cyan
Write-Host -NoNewline "Branch         : "; Write-Host $Branch        -foregroundColor cyan

Out-File ".vers.new"         -InputObject "#ifndef MYTH_SOURCE_VERSION"
Out-File ".vers.new" -Append -InputObject "#define MYTH_SOURCE_VERSION `"$SourceVersion`""
Out-File ".vers.new" -Append -InputObject "#define MYTH_SOURCE_PATH    `"$branch`""
Out-File ".vers.new" -Append -InputObject "#endif"

$bCopy = $false

# does version.h exist?

if (-not (Test-Path $RootDir\libs\libmythbase\version.h ))
{
    $bCopy = $true
}
else
{
    # check if the version strings are changed and update version.h if necessary

    $results = Compare-Object $(Get-Content .vers.new )                  `
                          $(Get-Content $RootDir\libs\libmythbase\version.h )

    if ($results.length -ne 0)
    {
        $bCopy = $true
    }
}

if ($bCopy -eq $true )
{
    Write-Host "Updating version.h" -foregroundColor Green
    Copy-Item .vers.new $RootDir\libs\libmythbase\version.h
}
else
{
    Write-Host "version.h Not Modifed."  -foregroundColor Green
}

Remove-Item .vers.new -ErrorAction SilentlyContinue

Write-Host 
