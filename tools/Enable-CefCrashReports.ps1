param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [string]$SubprocessPath,
    [switch]$Disable
)
$ErrorActionPreference = 'Stop'
# Run only when Skyrim is closed. This is an explicit diagnostic setup tool,
# not part of the build/install workflow and not a replacement for JSON settings.
$crashGameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path
$crashGameRoot = [IO.Path]::GetFullPath($crashGameRoot)
if (!(Test-Path -LiteralPath (Join-Path $crashGameRoot 'SkyrimSE.exe')) -and
    !(Test-Path -LiteralPath (Join-Path $crashGameRoot 'SkyrimVR.exe'))) {
    throw 'GameDirectory must contain SkyrimSE.exe or SkyrimVR.exe.'
}
$crashConfig = [IO.Path]::GetFullPath((Join-Path $crashGameRoot 'crash_reporter.cfg'))
if ([IO.Path]::GetDirectoryName($crashConfig) -ne $crashGameRoot.TrimEnd('\')) { throw 'Unexpected crash reporter path.' }
$crashMarker = '# Managed by SkyUltra: local-only CEF crash reports'
if (Test-Path -LiteralPath $crashConfig) {
    if (!(Get-Content -LiteralPath $crashConfig -Raw).StartsWith($crashMarker)) {
        throw 'An existing crash_reporter.cfg belongs to another application/mod. It was not changed.'
    }
}
if ($Disable) {
    if (Test-Path -LiteralPath $crashConfig) { Remove-Item -LiteralPath $crashConfig }
    Write-Output 'SkyUltra CEF crash reporting disabled for the next game launch.'
    return
}
if (!$SubprocessPath) { $SubprocessPath = Join-Path $crashGameRoot 'Data/SKSE/Plugins/SkyUltra/CEF/SkyUltraCefSubprocess.exe' }
$crashHelper = (Resolve-Path -LiteralPath $SubprocessPath).Path
if ([IO.Path]::GetFileName($crashHelper) -ne 'SkyUltraCefSubprocess.exe') { throw 'SubprocessPath must name SkyUltraCefSubprocess.exe.' }
if ($crashHelper.Contains("`n") -or $crashHelper.Contains("`r")) { throw 'Invalid subprocess path.' }
$crashText = @"
$crashMarker
[Config]
ProductName=SkyUltra
ProductVersion=0.1.0.0
AppName=SkyUltraCEF
ExternalHandler=$crashHelper
ServerURL=
RateLimitEnabled=true
MaxUploadsPerDay=5
MaxDatabaseSizeInMb=128
MaxDatabaseAgeInDays=7

[CrashKeys]
skyultra-process=small
skyultra-view=small
skyultra-operation=small
"@
[IO.File]::WriteAllText($crashConfig, $crashText, [Text.UTF8Encoding]::new($false))
Write-Output "Enabled local-only reports: $crashConfig"
Write-Output 'Restart Skyrim. Check SkyUltra.log for CEF crash reporting enabled=true.'
Write-Output 'Reports are kept under %LOCALAPPDATA%\SkyUltraCEF\User Data. No upload server is configured.'
