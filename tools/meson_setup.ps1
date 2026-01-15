[CmdletBinding()]
param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$Args
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$windres = Join-Path $scriptDir 'rc.cmd'
if (-not (Test-Path -Path $windres)) {
  throw "WINDRES wrapper not found at $windres"
}

$env:WINDRES = $windres
& meson @Args
exit $LASTEXITCODE
