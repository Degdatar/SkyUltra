param(
    [string]$GameDirectory = 'D:\Steam\steamapps\common\Skyrim Special Edition',
    [switch]$SkipDependencyInstall
)
Write-Host 'The test UI is now the built-in menu. Forwarding to Build-SkyUltraMenu.ps1.'
& (Join-Path $PSScriptRoot 'Build-SkyUltraMenu.ps1') -GameDirectory $GameDirectory -SkipDependencyInstall:$SkipDependencyInstall
