<#
.SYNOPSIS
Package plugins from a finished build into distributable ZIPs (PowerShell counterpart of package.sh).

.DESCRIPTION
  ./package.ps1 [plugin_dir ...]   default: every directory under plugins/ that has a manifest.json
Build dir per plugin: $env:BUILD_DIR if set, else build/<plugin> (./build.ps1 <plugin>), else build/all (./build.ps1).
Output: dist/<id>-<version>-<os>-<arch>.zip with a single top-level <id>/ entry (PJ requirement). Files in
plugins/<p>/resources/ (if present) are copied into <id>/ too. $env:ARCH overrides the detected architecture.
Validates manifest.json (incl. min_sdk_required <= SDK_VERSION) and the compiled binary's embedded manifest.
#>
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot

function Invoke-Native {
    # Run a native command, stop on a non-zero exit code, and return what it wrote to stdout.
    param([string]$Exe, [string[]]$Arguments)
    # Windows PowerShell 5.1 turns anything a native tool writes to stderr into an error under 'Stop'.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Exe @Arguments } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

$OnWindows = ($PSVersionTable.PSVersion.Major -lt 6) -or $IsWindows
if ($OnWindows) {
    $OsLabel = 'windows'
    $Machine = switch ($env:PROCESSOR_ARCHITECTURE) { 'AMD64' { 'x86_64' } 'ARM64' { 'aarch64' } default { $env:PROCESSOR_ARCHITECTURE } }
} else {
    $OsLabel = if ($IsMacOS) { 'macos' } else { 'linux' }
    $Machine = (& uname -m).Trim()
}
$Arch = if ($env:ARCH) { $env:ARCH } else { $Machine }

# Skip the Microsoft Store "python3" stub in WindowsApps: it is not a real interpreter.
$Py = foreach ($name in 'python3', 'python') {
    $cmd = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue |
        Where-Object { $_.Source -notmatch 'WindowsApps' } | Select-Object -First 1
    if ($cmd) { $cmd.Source; break }
}
if (-not $Py) { throw 'python3/python not found on PATH' }

$Plugins = @($args)
if ($Plugins.Count -eq 0) {
    $Plugins = @(Get-ChildItem (Join-Path $Root 'plugins') -Directory |
            Where-Object { Test-Path (Join-Path $_.FullName 'manifest.json') } | ForEach-Object Name)
}

$ReleaseTools = Join-Path $Root 'scripts/release_tools.py'
$Dist = Join-Path $Root 'dist'

# Same zip writer as package.sh: PJ requires a single top-level entry named after the plugin id.
$ZipScript = @'
import sys, zipfile
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
dst.unlink(missing_ok=True)
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zf:
    top = src.name
    zf.writestr(top + "/", "")
    for f in sorted(src.rglob("*")):
        arc = f"{top}/{f.relative_to(src).as_posix()}"
        if f.is_dir():
            zf.writestr(arc + "/", "")
        else:
            zf.write(f, arc)
print(f"Created {dst}")
'@

foreach ($p in $Plugins) {
    $manifest = Join-Path $Root "plugins/$p/manifest.json"
    if ($env:BUILD_DIR) { $bdir = $env:BUILD_DIR }
    elseif (Test-Path (Join-Path $Root "build/$p/bin")) { $bdir = Join-Path $Root "build/$p" }
    else { $bdir = Join-Path $Root 'build/all' }

    Invoke-Native $Py @($ReleaseTools, 'validate-manifest', $manifest) | Write-Host
    $id = (Get-Content $manifest -Raw | ConvertFrom-Json).id
    $stage = Join-Path $Dist $id
    # Drop leftovers from earlier runs so they don't end up in the ZIP.
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }

    # release_tools stages dist/<id>/ and prints the zip name on stdout (it never writes the zip itself).
    $zipName = @(Invoke-Native $Py @($ReleaseTools, 'create-distribution-package', $p,
            '--build-dir', (Join-Path $bdir 'bin'), '--output-dir', $Dist,
            '--os-label', $OsLabel, '--arch', $Arch))[-1]

    # Optional plugins/<p>/resources/ (xml, etc.) is copied into the ZIP root as-is.
    $resources = Join-Path $Root "plugins/$p/resources"
    if (Test-Path $resources) {
        Get-ChildItem $resources -Force | Copy-Item -Destination $stage -Recurse -Force
    }

    $ZipScript | & $Py - $stage (Join-Path $Dist $zipName)
    if ($LASTEXITCODE -ne 0) { throw "zip creation failed for $p" }
}
