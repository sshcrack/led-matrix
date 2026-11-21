# One-shot card flashing for Windows: starts Raspberry Pi Imager with this
# project's image list. Installs Imager first (its normal installer) if missing.
#
#   irm https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/flash_card.ps1 | iex
$ErrorActionPreference = 'Stop'
$repo = if ($env:LED_MATRIX_IMAGER_REPO) { $env:LED_MATRIX_IMAGER_REPO } else { 'https://github.com/sshcrack/led-matrix/releases/latest/download/led-matrix-imager.json' }
$candidates = @(
  "$env:LOCALAPPDATA\Programs\Raspberry Pi Imager\rpi-imager.exe",
  "$env:ProgramFiles\Raspberry Pi Imager\rpi-imager.exe",
  "${env:ProgramFiles(x86)}\Raspberry Pi Imager\rpi-imager.exe")
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
  $setup = Join-Path $env:TEMP 'imager_latest.exe'
  Write-Host 'Downloading Raspberry Pi Imager...'
  Invoke-WebRequest 'https://downloads.raspberrypi.com/imager/imager_latest.exe' -OutFile $setup
  Write-Host 'Running the Imager installer (finish it, then Imager starts)...'
  Start-Process $setup -Wait
  $exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $exe) { throw 'Imager was not found after installing. Start it once, then run this again.' }
}
Write-Host 'Starting Raspberry Pi Imager with the LED Matrix image list...'
& $exe --repo $repo
