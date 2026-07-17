# SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$wifiIni = Join-Path $repositoryRoot 'examples/client-demo-wifi/platformio.ini'
$ethernetIni = Join-Path $repositoryRoot 'examples/client-demo-ETH/platformio.ini'
$demoMain = Join-Path $repositoryRoot 'examples/common/client-demo/src/main.cpp'
$browserSource = Join-Path $repositoryRoot 'examples/common/client-demo/src/BacnetDemoPropertyBrowser.cpp'

function Require-Text {
  param([string]$Text, [string]$Pattern, [string]$Message)
  if ($Text -notmatch $Pattern) {
    throw $Message
  }
}

function Reject-Text {
  param([string]$Text, [string]$Pattern, [string]$Message)
  if ($Text -match $Pattern) {
    throw $Message
  }
}

$wifi = Get-Content -Raw -LiteralPath $wifiIni
$ethernet = Get-Content -Raw -LiteralPath $ethernetIni
$main = Get-Content -Raw -LiteralPath $demoMain
$browser = Get-Content -Raw -LiteralPath $browserSource

foreach ($ini in @($wifi, $ethernet)) {
  Require-Text $ini '(?m)^src_dir\s*=\s*\.\./common/client-demo/src\s*$' `
    'Both rich demos must use the shared client-demo source directory.'
  Require-Text $ini '-DESP_BACNET_ENABLE_WRITE_PROPERTY=1' `
    'Both rich demos must explicitly enable WriteProperty.'
  Require-Text $ini '-DESP_BACNET_ENABLE_PRIORITY_WRITE=1' `
    'Both rich demos must explicitly enable priority writes.'
}

Reject-Text $wifi '(?m)^\[env:usb_write\]\s*$' `
  'The obsolete WiFi usb_write environment must not be reintroduced.'
foreach ($removedFallback in @(
    'appendConfiguredFallbackObjects',
    'readConfiguredFallbackObject',
    'Configured fallback objects loaded',
    'fallbackScanBuffer')) {
  Reject-Text $main ([regex]::Escape($removedFallback)) `
    "Object-list scans must not restore the removed automatic fallback ($removedFallback)."
}
foreach ($blockingRead in @('readPropertyList\s*\(', 'readAllProperties\s*\(')) {
  Reject-Text $browser $blockingRead `
    'The Property Browser must use its incremental read job instead of a blocking bulk read.'
}

Write-Host 'Client demo parity and non-blocking browser checks passed.'
