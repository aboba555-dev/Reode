# Copyright 2026 The Reode Authors
#
# Installs Reode into a Geometry Dash directory.
#
#   install.ps1 -GameDir "C:\...\Geometry Dash" -BuildDir build\loader\Debug
#
# An existing XInput1_4.dll that is not Reode's (for example Geode's) is
# renamed to XInput1_4.dll.reode-backup so it can be restored later with
# -Uninstall.

param(
  [Parameter(Mandatory = $true)][string]$GameDir,
  [string]$BuildDir = "",
  [switch]$Uninstall
)

$ErrorActionPreference = "Stop"
$proxy = Join-Path $GameDir "XInput1_4.dll"
$backup = "$proxy.reode-backup"

if (-not (Test-Path (Join-Path $GameDir "GeometryDash.exe"))) {
  throw "GeometryDash.exe not found in $GameDir"
}

function Test-IsReodeProxy([string]$path) {
  if (-not (Test-Path $path)) { return $false }
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $text = [System.Text.Encoding]::ASCII.GetString($bytes)
  return $text.Contains("ReodeInitialize")
}

if ($Uninstall) {
  if (Test-IsReodeProxy $proxy) { Remove-Item $proxy }
  if (Test-Path $backup) { Move-Item $backup $proxy }
  Remove-Item (Join-Path $GameDir "Reode.dll") -ErrorAction SilentlyContinue
  Write-Host "Reode removed. reode\ (mods, saves, logs) was kept."
  return
}

$root = Split-Path $BuildDir -Parent | Split-Path -Parent
$config = Split-Path $BuildDir -Leaf
$files = @{
  "Reode.dll" = Join-Path $BuildDir "Reode.dll"
  "XInput1_4.dll" = Join-Path $root "proxy\$config\XInput1_4.dll"
  "reode\compat\Geode.dll" = Join-Path $root "geode_shim\$config\Geode.dll"
}

if ((Test-Path $proxy) -and -not (Test-IsReodeProxy $proxy) -and
    -not (Test-Path $backup)) {
  Move-Item $proxy $backup
  Write-Host "Backed up the previous XInput1_4.dll to $backup"
}

# Geode.dll exists only in builds with -DREODE_GEODE_COMPAT=ON.
if (-not (Test-Path $files["reode\compat\Geode.dll"])) {
  $files.Remove("reode\compat\Geode.dll")
}

foreach ($entry in $files.GetEnumerator()) {
  $destination = Join-Path $GameDir $entry.Key
  New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
  Copy-Item $entry.Value $destination -Force
  Write-Host "Installed $($entry.Key)"
}
