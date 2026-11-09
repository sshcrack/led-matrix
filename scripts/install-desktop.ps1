# Installs the LED Matrix Controller desktop app for the current user (no administrator rights):
#
#   irm https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/install-desktop.ps1 | iex
#
# Same as running the release's led-matrix-desktop-<version>-win64.exe: it installs to
# %LOCALAPPDATA%\led-matrix-desktop and keeps itself up to date from then on.
# Set $env:LED_MATRIX_VERSION = "1.21.0" first to install a specific version.
$ErrorActionPreference = 'Stop'
$repo = 'sshcrack/led-matrix'

$version = $env:LED_MATRIX_VERSION
if (-not $version) {
    $version = (Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest").tag_name
}
$version = $version.TrimStart('v')

$asset = "led-matrix-desktop-$version-win64.exe"
$setup = Join-Path ([IO.Path]::GetTempPath()) $asset
Write-Host "Downloading LED Matrix Controller $version..."
$ProgressPreference = 'SilentlyContinue' # Invoke-WebRequest is very slow with the progress bar
Invoke-WebRequest "https://github.com/$repo/releases/download/v$version/$asset" -OutFile $setup
try {
    $p = Start-Process -FilePath $setup -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "Setup failed with exit code $($p.ExitCode)" }
    Write-Host "Installed. LED Matrix Controller keeps itself up to date."
} finally {
    Remove-Item $setup -ErrorAction SilentlyContinue
}
