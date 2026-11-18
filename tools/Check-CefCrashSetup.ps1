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

    # Verify StageCefRuntime.cmake prunes stale/extra locales and moves subprocess PDBs to Diagnostics/.
    $stageRoot = Join-Path $crashTestRoot 'stage-check'
    $binSrc = Join-Path $stageRoot 'bin'
    $resSrc = Join-Path $stageRoot 'res'
    $rootSrc = Join-Path $stageRoot 'cefroot'
    $pkgDst = Join-Path $stageRoot 'pkg/Data/SKSE/Plugins/SkyUltra/CEF'
    $diagDst = Join-Path $stageRoot 'pkg/Diagnostics'
    $null = New-Item -ItemType Directory -Path $binSrc, (Join-Path $resSrc 'locales'), $rootSrc, (Join-Path $pkgDst 'Resources/locales'), (Join-Path $pkgDst 'Release'), (Join-Path $pkgDst 'locales'), $diagDst -Force
    foreach ($f in @('chrome_elf.dll','d3dcompiler_47.dll','dxcompiler.dll','dxil.dll','libcef.dll','v8_context_snapshot.bin','vk_swiftshader.dll','vk_swiftshader_icd.json','vulkan-1.dll')) {
        [IO.File]::WriteAllBytes((Join-Path $binSrc $f), [byte[]]@(1))
    }
    foreach ($f in @('chrome_100_percent.pak','chrome_200_percent.pak','icudtl.dat','resources.pak')) {
        [IO.File]::WriteAllBytes((Join-Path $resSrc $f), [byte[]]@(2))
    }
    [IO.File]::WriteAllBytes((Join-Path $resSrc 'locales/en-US.pak'), [byte[]]@(3))
    [IO.File]::WriteAllBytes((Join-Path $resSrc 'locales/fr.pak'), [byte[]]@(4))
    [IO.File]::WriteAllBytes((Join-Path $resSrc 'locales/af_FEMININE.pak'), [byte[]]@(5))
    [IO.File]::WriteAllText((Join-Path $rootSrc 'LICENSE.txt'), 'BSD')
    [IO.File]::WriteAllText((Join-Path $rootSrc 'README.txt'), 'SDK')
    [IO.File]::WriteAllBytes((Join-Path $pkgDst 'SkyUltraCefSubprocess.exe'), [byte[]]@(6))
    [IO.File]::WriteAllBytes((Join-Path $pkgDst 'SkyUltraCefSubprocess.pdb'), [byte[]]@(7))
    [IO.File]::WriteAllText((Join-Path $pkgDst 'README.txt'), 'OLD')
    [IO.File]::WriteAllBytes((Join-Path $pkgDst 'Resources/locales/af_MASCULINE.pak'), [byte[]]@(8))
    [IO.File]::WriteAllBytes((Join-Path $pkgDst 'Release/SkyUltraCefSubprocess.exe'), [byte[]]@(9))
    [IO.File]::WriteAllBytes((Join-Path $pkgDst 'locales/de.pak'), [byte[]]@(10))
    $cmakeScript = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../cmake/StageCefRuntime.cmake'))
    & cmake "-DCEF_BINARY_SOURCE=$binSrc" "-DCEF_RESOURCE_SOURCE=$resSrc" "-DCEF_ROOT_SOURCE=$rootSrc" "-DCEF_PACKAGE_DESTINATION=$pkgDst" "-DCEF_DIAGNOSTICS_DESTINATION=$diagDst" -P $cmakeScript
    if ($LASTEXITCODE -ne 0) { throw 'StageCefRuntime.cmake failed.' }
    $stagedFiles = @(Get-ChildItem -LiteralPath $pkgDst -Recurse -File | ForEach-Object { $_.FullName.Substring($pkgDst.Length + 1).Replace('\', '/') } | Sort-Object)
    $expectedFiles = @('chrome_100_percent.pak','chrome_200_percent.pak','chrome_elf.dll','d3dcompiler_47.dll','dxcompiler.dll','dxil.dll','icudtl.dat','libcef.dll','LICENSE.txt','locales/en-US.pak','resources.pak','SkyUltraCefSubprocess.exe','v8_context_snapshot.bin','vk_swiftshader.dll','vk_swiftshader_icd.json','vulkan-1.dll') | Sort-Object
    if (($stagedFiles -join ',') -ne ($expectedFiles -join ',')) {
        throw "Unexpected staged CEF layout: $($stagedFiles -join ', ')"
    }
    if (!(Test-Path -LiteralPath (Join-Path $diagDst 'SkyUltraCefSubprocess.pdb'))) {
        throw 'SkyUltraCefSubprocess.pdb was not moved to Diagnostics/.'
    }
    Write-Output "CEF runtime staging check passed: exactly $($stagedFiles.Count) files in CEF package, PDB in Diagnostics/."
} finally {
    $crashResolvedTestRoot = [IO.Path]::GetFullPath($crashTestRoot)
    if (!$crashResolvedTestRoot.StartsWith($crashTestWorkspace + [IO.Path]::DirectorySeparatorChar)) { throw 'Unsafe test cleanup path.' }
    if (Test-Path -LiteralPath $crashResolvedTestRoot) { Remove-Item -LiteralPath $crashResolvedTestRoot -Recurse -Force }
}
