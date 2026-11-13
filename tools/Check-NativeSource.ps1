param([string[]]$Source = @('src/render/ActorPreview.cpp'), [string[]]$Include = @())
$ErrorActionPreference = 'Stop'
# MSVC syntax checking only: no /c, /Fo, /Fd, /Zi, PCH creation or linking.
# Reuse an existing CMake configuration; this script never configures/builds it.
$syntaxRoot = Split-Path -Parent $PSScriptRoot
$syntaxProject = [xml](Get-Content -LiteralPath (Join-Path $syntaxRoot 'build/SkyUltra.vcxproj') -Raw)
$syntaxRelease = $syntaxProject.Project.ItemDefinitionGroup |
    Where-Object { $_.Condition -match "'Release\|x64'" } | Select-Object -First 1
if (!$syntaxRelease) { throw 'Existing Release compiler settings are unavailable.' }
$syntaxVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $syntaxVsWhere)) { throw 'Visual Studio Installer/vswhere.exe is required to locate MSVC.' }
$syntaxVsDirectory = & $syntaxVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$syntaxVsDirectory) { throw 'No Visual Studio installation with the C++ tools was found.' }
$syntaxVcRoot = Join-Path $syntaxVsDirectory 'VC/Tools/MSVC'
$syntaxVcVersion = Get-ChildItem -LiteralPath $syntaxVcRoot -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$syntaxCompiler = Join-Path $syntaxVcVersion.FullName 'bin/Hostx64/x64/cl.exe'
$syntaxKitVersion = Get-ChildItem -LiteralPath 'C:\Program Files (x86)\Windows Kits\10\Include' -Directory |
    Where-Object Name -Match '^10\.' | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$syntaxIncludeRoots = @((Join-Path $syntaxVcVersion.FullName 'include')) +
    @('ucrt','shared','um','winrt','cppwinrt' | ForEach-Object { Join-Path $syntaxKitVersion.FullName $_ })
$syntaxArguments = @('/nologo','/Zs','/Y-','/MD','/EHsc','/std:c++latest','/Zc:preprocessor','/W1','/utf-8','/TP')
$syntaxArguments += $syntaxRelease.ClCompile.AdditionalIncludeDirectories.Split(';') |
    Where-Object { $_ -and !$_.StartsWith('%(') } | ForEach-Object { '/I' + $_ }
# CMake's system include directories may be recorded separately from /I.
foreach ($syntaxRelative in @('_deps/vcpkg_installed/x64-windows-static-md/include',
    '_deps/vcpkg_installed/x64-windows-static-md/include/directxtk','_deps/cef','../include')) {
    $syntaxArguments += '/I' + (Join-Path $syntaxRoot $syntaxRelative)
}
$syntaxArguments += $syntaxRelease.ClCompile.PreprocessorDefinitions.Split(';') |
    Where-Object { $_ -and !$_.StartsWith('%(') -and !$_.StartsWith('CMAKE_INTDIR=') } | ForEach-Object { '/D' + $_ }
$syntaxArguments += '/FI' + (Join-Path $syntaxRoot 'build/CMakeFiles/SkyUltra.dir/Release/cmake_pch.hxx')
$syntaxArguments += $Include | ForEach-Object { '/I' + (Resolve-Path -LiteralPath (Join-Path $syntaxRoot $_)).Path }
$syntaxPreviousInclude = $env:INCLUDE
$syntaxFailures = 0
try {
    $env:INCLUDE = $syntaxIncludeRoots -join ';'
    foreach ($syntaxSource in $Source) {
        $syntaxFile = (Resolve-Path -LiteralPath (Join-Path $syntaxRoot $syntaxSource)).Path
        Write-Output "MSVC /Zs: $syntaxSource (no binary output)"
        $fileArgs = $syntaxArguments
        if ($syntaxSource -match 'VideoSplash\.cpp$') {
            $fileArgs = $syntaxArguments | Where-Object { $_ -notlike '/FI*cmake_pch*' -and $_ -notlike '/DWINVER=*' -and $_ -notlike '/D_WIN32_WINNT=*' }
            $fileArgs += @('/DWINVER=0x0602', '/D_WIN32_WINNT=0x0602')
        }
        & $syntaxCompiler @fileArgs $syntaxFile
        if ($LASTEXITCODE -ne 0) { $syntaxFailures++ }
    }
} finally { $env:INCLUDE = $syntaxPreviousInclude }
if ($syntaxFailures) { throw "$syntaxFailures source file(s) failed syntax checking." }
