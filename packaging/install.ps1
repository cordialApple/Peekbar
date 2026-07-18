<#
.SYNOPSIS
    Install Peekbar for the current user and register per-user logon autostart.
.DESCRIPTION
    Copies peekbar.exe to %LOCALAPPDATA%\Peekbar and adds a "Peekbar" value under
    HKCU\Software\Microsoft\Windows\CurrentVersion\Run so it starts at logon.
    Per-user only: HKCU, no HKLM, no elevation, nothing system-wide.
    Supports -WhatIf to preview without changing anything.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\install.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\install.ps1 -NoAutostart
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Source = (Join-Path $PSScriptRoot 'peekbar.exe'),
    [string]$SampleConfig = (Join-Path $PSScriptRoot 'config.sample.txt'),
    [switch]$NoAutostart
)

$ErrorActionPreference = 'Stop'

$installDir = Join-Path $env:LOCALAPPDATA 'Peekbar'
$target     = Join-Path $installDir 'peekbar.exe'
$runKey     = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'

if (-not (Test-Path -LiteralPath $Source)) {
    throw "peekbar.exe not found at '$Source'. Run this script from the unzipped folder."
}

if ($PSCmdlet.ShouldProcess($installDir, 'Create install dir and copy peekbar.exe')) {
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
    Copy-Item -LiteralPath $Source -Destination $target -Force
    Write-Host "Installed peekbar.exe to $target"
}

if (Test-Path -LiteralPath $SampleConfig) {
    $sampleTarget = Join-Path $installDir 'config.sample.txt'
    if ($PSCmdlet.ShouldProcess($sampleTarget, 'Copy config.sample.txt next to the exe')) {
        Copy-Item -LiteralPath $SampleConfig -Destination $sampleTarget -Force
        Write-Host "Copied config.sample.txt to $sampleTarget (copy it to config.txt to add buttons)."
    }
} else {
    Write-Host "config.sample.txt not found at '$SampleConfig'; skipped seeding the sample."
}

if (-not $NoAutostart) {
    if ($PSCmdlet.ShouldProcess($runKey, "Register autostart value 'Peekbar'")) {
        New-ItemProperty -Path $runKey -Name 'Peekbar' -Value ('"{0}"' -f $target) `
            -PropertyType String -Force | Out-Null
        Write-Host "Registered logon autostart (HKCU Run value 'Peekbar')."
    }
} else {
    Write-Host "Skipped autostart (-NoAutostart)."
}
