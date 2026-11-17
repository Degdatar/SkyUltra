$ErrorActionPreference = 'Stop'
# Exercise the real setup script against workspace-only placeholder paths.
# Neither Skyrim nor a CEF executable is launched; no registry changes are made.
$crashTestWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../.diagnostics'))
$crashTestRoot = Join-Path $crashTestWorkspace ('cef-crash-setup-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $crashTestRoot -Force
$crashFakeGame = Join-Path $crashTestRoot 'SkyrimSE.exe'
$crashFakeHelper = Join-Path $crashTestRoot 'SkyUltraCefSubprocess.exe'
$crashTestConfig = Join-Path $crashTestRoot 'crash_reporter.cfg'
[IO.File]::WriteAllBytes($crashFakeGame,[byte[]]@())
[IO.File]::WriteAllBytes($crashFakeHelper,[byte[]]@())
try {
    $crashSetup = Join-Path $PSScriptRoot 'Enable-CefCrashReports.ps1'
    & $crashSetup -GameDirectory $crashTestRoot -SubprocessPath $crashFakeHelper | Out-Null
    $crashContents = Get-Content -LiteralPath $crashTestConfig -Raw
    if ($crashContents -notmatch '(?m)^ServerURL=\r?$' -or $crashContents -notmatch 'MaxDatabaseSizeInMb=128' -or
        !$crashContents.Contains("ExternalHandler=$crashFakeHelper")) { throw 'Crash configuration did not remain local and bounded.' }
    & $crashSetup -GameDirectory $crashTestRoot -Disable | Out-Null
    if (Test-Path -LiteralPath $crashTestConfig) { throw 'Managed crash configuration was not removed.' }
    [IO.File]::WriteAllText($crashTestConfig,'# Another application owns this file')
    $crashRefused = $false
    try { & $crashSetup -GameDirectory $crashTestRoot -SubprocessPath $crashFakeHelper | Out-Null } catch { $crashRefused = $true }
    if (!$crashRefused -or (Get-Content -LiteralPath $crashTestConfig -Raw) -ne '# Another application owns this file') {
        throw 'Existing foreign crash configuration was overwritten.'
    }
    Write-Output 'CEF crash setup checks passed: local-only configuration, bounded retention, disable, and refusal to overwrite another owner. No game/runtime executed.'
} finally {
    $crashResolvedTestRoot = [IO.Path]::GetFullPath($crashTestRoot)
    if (!$crashResolvedTestRoot.StartsWith($crashTestWorkspace + [IO.Path]::DirectorySeparatorChar)) { throw 'Unsafe test cleanup path.' }
    foreach ($crashTestFile in @($crashFakeGame,$crashFakeHelper,$crashTestConfig)) {
        if (Test-Path -LiteralPath $crashTestFile) { Remove-Item -LiteralPath $crashTestFile }
    }
    Remove-Item -LiteralPath $crashResolvedTestRoot
}
