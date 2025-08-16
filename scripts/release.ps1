#! /usr/bin/env pwsh
$ErrorActionPreference = "Stop"
# Show current version and prompt for new version

$CMakeLists = "CMakeLists.txt"

# Extract current version from CMakeLists.txt
$currentVersion = ""
$cmakeContent = Get-Content $CMakeLists
foreach ($line in $cmakeContent) {
    if ($line -match '^project\(([^)]*)VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $currentVersion = $Matches[2]
        break
    }
}

if (-not $currentVersion) {
    Write-Error "Could not find project() line with version in $CMakeLists"
    exit 1
}

Write-Host "Current version is $currentVersion"
$NewVersion = Read-Host "Enter new version"

if (-not $NewVersion -or $NewVersion -eq $currentVersion) {
    Write-Error "No new version provided or version unchanged."
    exit 1
}

# Update version in CMakeLists.txt
$updated = $false
$cmakeContent = $cmakeContent | ForEach-Object {
    if ($_ -match '^(\s*project\([^)]*VERSION\s+)([0-9]+\.[0-9]+\.[0-9]+)(.*)$') {
        $updated = $true
        return "$($Matches[1])$NewVersion$($Matches[3])"
    } else {
        return $_
    }
}
if (-not $updated) {
    Write-Error "Could not update version in $CMakeLists"
    exit 1
}
$cmakeContent | Set-Content $CMakeLists
Write-Host "Updated $CMakeLists to version $NewVersion"

# Detect platform and choose preset
if ($IsWindows) {
    $preset = "desktop-windows"
} else {
    $preset = "desktop-linux"
}
Write-Host "Configuring with preset: $preset"
cmake --preset $preset

# Commit and push changes
git add $CMakeLists
git commit -m "Bump to version $NewVersion"
git push

# Get GitHub repo (format: owner/repo)
$remoteUrl = git config --get remote.origin.url
if ($remoteUrl -match "github.com[:/](.+?)(\.git)?$") {
    $repo = $Matches[1]
} else {
    Write-Error "Could not determine GitHub repository."
    exit 1
}

Write-Host "Triggering create-release.yml workflow on $repo"
gh workflow run create-release.yml -R $repo

Write-Host "Release workflow triggered successfully."