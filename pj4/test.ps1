<#
.SYNOPSIS
Run ctest in every build directory produced by build.ps1 / build.sh (PowerShell counterpart of test.sh).
#>
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot

$caches = @(Get-ChildItem -Path (Join-Path $Root 'build/*/CMakeCache.txt'), (Join-Path $Root 'build/*/debug/CMakeCache.txt') `
        -ErrorAction SilentlyContinue)
if ($caches.Count -eq 0) { throw 'No build directory found; run ./build.ps1 first' }

foreach ($cache in $caches) {
    $dir = $cache.DirectoryName
    $line = Select-String -Path $cache.FullName -Pattern '^CMAKE_BUILD_TYPE:' | Select-Object -First 1
    $cfg = if ($line) { ($line.Line -split '=', 2)[1] } else { '' }
    if (-not $cfg) { $cfg = 'Release' }

    # Windows PowerShell 5.1 turns anything a native tool writes to stderr into an error under 'Stop'.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & ctest --test-dir $dir -C $cfg --output-on-failure } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "ctest failed in $dir (exit code $LASTEXITCODE)" }
}
