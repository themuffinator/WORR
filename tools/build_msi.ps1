param(
  [Parameter(Mandatory = $true)]
  [string]$SourceDir,
  [Parameter(Mandatory = $true)]
  [string]$OutputDir,
  [Parameter(Mandatory = $true)]
  [string]$Version,
  [string]$ProductName = "WORR",
  [string]$Manufacturer = "DarkMatter Productions",
  [string]$UpgradeCode = "{0AA7041A-0DC8-43A8-90E7-87D83A07B696}",
  [string]$MsiName = "worr-win64.msi"
)

$ErrorActionPreference = "Stop"

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
  throw "Version must be Major.Minor.Patch for MSI. Got: $Version"
}

$heat = Get-Command heat.exe -ErrorAction SilentlyContinue
$candle = Get-Command candle.exe -ErrorAction SilentlyContinue
$light = Get-Command light.exe -ErrorAction SilentlyContinue
if (-not $heat -or -not $candle -or -not $light) {
  throw "WiX Toolset executables not found in PATH."
}

$source = (Resolve-Path $SourceDir).Path
$output = (Resolve-Path $OutputDir).Path
New-Item -ItemType Directory -Force -Path $output | Out-Null

$tempRoot = Join-Path $env:TEMP ("worr_msi_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$wxsTemplate = Join-Path $PSScriptRoot "installer\worr.wxs"
$productWxs = Join-Path $tempRoot "Product.wxs"
$harvestWxs = Join-Path $tempRoot "Harvest.wxs"

Copy-Item $wxsTemplate $productWxs

& $heat.Source dir $source `
  -cg AppComponents `
  -dr INSTALLDIR `
  -gg -g1 `
  -scom -sreg -srd `
  -var var.SourceDir `
  -out $harvestWxs

& $candle.Source -nologo `
  -dProductVersion=$Version `
  -dProductName="$ProductName" `
  -dManufacturer="$Manufacturer" `
  -dUpgradeCode=$UpgradeCode `
  -dSourceDir="$source" `
  -out "$tempRoot\" `
  $productWxs $harvestWxs

$msiPath = Join-Path $output $MsiName
& $light.Source -nologo `
  -ext WixUIExtension `
  -out $msiPath `
  (Join-Path $tempRoot "Product.wixobj") `
  (Join-Path $tempRoot "Harvest.wixobj")

Remove-Item -Recurse -Force $tempRoot

Write-Host "Wrote $msiPath"
