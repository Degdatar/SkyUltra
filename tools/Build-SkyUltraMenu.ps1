param(
    [string]$GameDirectory = 'D:\Steam\steamapps\common\Skyrim Special Edition',
    [switch]$SkipDependencyInstall,
    [switch]$SkipNative
)
$ErrorActionPreference = 'Stop'
$nativeRoot = Split-Path -Parent $PSScriptRoot
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')" }
}
$npm = (Get-Command npm.cmd -ErrorAction Stop).Source
foreach ($project in @('sdk\typescript', 'sdk\ui', 'apps\skyultra-menu')) {
    Push-Location (Join-Path $nativeRoot $project)
    try {
        if (!$SkipDependencyInstall) { Invoke-Checked $npm @('install') }
        Invoke-Checked $npm @('run', 'build')
    } finally { Pop-Location }
}
if (!$SkipNative) {
    $papyrusRoot = Join-Path $nativeRoot 'sdk\papyrus'
    $compiler = Join-Path $GameDirectory 'Papyrus Compiler\PapyrusCompiler.exe'
    $flags = Join-Path $GameDirectory 'Data\Source\Scripts\TESV_Papyrus_Flags.flg'
    foreach ($required in @($compiler, $flags)) {
        if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required build input missing: $required" }
    }
    $pexOutput = Join-Path $papyrusRoot 'Compiled'
    New-Item -ItemType Directory -Path $pexOutput -Force | Out-Null
    $imports = @((Join-Path $papyrusRoot 'Source\Scripts'), (Join-Path $GameDirectory 'Data\Scripts\Source'), (Join-Path $GameDirectory 'Data\Source\Scripts')) -join ';'
    Invoke-Checked $compiler @((Join-Path $papyrusRoot 'Source\Scripts\SkyUltraNative.psc'), "-f=$flags", "-i=$imports", "-o=$pexOutput")
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
    if (!$env:VCPKG_ROOT -and (Test-Path -LiteralPath 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake')) { $env:VCPKG_ROOT = 'C:\vcpkg' }
    Push-Location $nativeRoot
    try {
        Invoke-Checked $cmake @('--preset', 'local', '-DVCPKG_MANIFEST_INSTALL=OFF')
        Invoke-Checked $cmake @('--build', '--preset', 'local-release')
    } finally { Pop-Location }
    $scripts = Join-Path $nativeRoot 'build\package\Data\Scripts'
    New-Item -ItemType Directory -Path $scripts -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $pexOutput 'SkyUltraNative.pex') -Destination $scripts -Force
}
$menuOutput = Join-Path $nativeRoot 'build\package\Data\SkyUltra\skyultra.menu'
New-Item -ItemType Directory -Path $menuOutput -Force | Out-Null
# Retire the previous generated assets (including old hash-suffixed chunks).
# This is this checkout's staging tree, never the installed mod or game tree.
$stagedAssets = Join-Path $menuOutput 'Files\assets'
if (Test-Path -LiteralPath $stagedAssets) {
    $expectedAssets = [System.IO.Path]::GetFullPath((Join-Path $nativeRoot 'build\package\Data\SkyUltra\skyultra.menu\Files\assets'))
    $resolvedAssets = (Resolve-Path -LiteralPath $stagedAssets).Path
    if (![string]::Equals($resolvedAssets, $expectedAssets, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clear an unexpected generated assets path: $resolvedAssets"
    }
    $checkedDirectory = Get-Item -LiteralPath $stagedAssets -Force
    while ($null -ne $checkedDirectory -and ![string]::Equals($checkedDirectory.FullName, $nativeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        if ($checkedDirectory.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw 'Refusing to clear generated assets through a linked directory.'
        }
        $checkedDirectory = $checkedDirectory.Parent
    }
    if ($null -eq $checkedDirectory) {
        throw 'Generated assets are outside the source checkout.'
    }
    Remove-Item -LiteralPath $resolvedAssets -Recurse -Force
}
Get-ChildItem -LiteralPath (Join-Path $nativeRoot 'apps\skyultra-menu\dist') | Copy-Item -Destination $menuOutput -Recurse -Force
$interfaceOutput = Join-Path $nativeRoot 'build\package\Data\Interface'
New-Item -ItemType Directory -Path $interfaceOutput -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_inventory.swf') -Destination (Join-Path $interfaceOutput 'skyultra_inventory.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_barter.swf') -Destination (Join-Path $interfaceOutput 'skyultra_barter.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_magic.swf') -Destination (Join-Path $interfaceOutput 'skyultra_magic.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_favorites.swf') -Destination (Join-Path $interfaceOutput 'skyultra_favorites.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_container.swf') -Destination (Join-Path $interfaceOutput 'skyultra_container.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_mannequin.swf') -Destination (Join-Path $interfaceOutput 'skyultra_mannequin.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_loot.swf') -Destination (Join-Path $interfaceOutput 'skyultra_loot.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_pickpocket.swf') -Destination (Join-Path $interfaceOutput 'skyultra_pickpocket.swf') -Force
Copy-Item -LiteralPath (Join-Path $nativeRoot 'resources\skyultra_compass.swf') -Destination (Join-Path $interfaceOutput 'skyultra_compass.swf') -Force
Write-Host "Package: $(Join-Path $nativeRoot 'build\package\Data')"
Write-Host 'Copy the Data contents into your SkyUltra mod manually.'
Write-Host 'No game or mod-manager files were changed.'
