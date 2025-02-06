param(
    $OutDir = ".\TTDDownload",
    [ValidateSet("x64", "x86", "arm64")]
    $Arch = "x64"
)

# Ensure the output directory exists
if (!(Test-Path $OutDir)) {
    $null = mkdir $OutDir
}

# Ensure the temp directory exists
$TempDir = Join-Path $OutDir "TempTtd"
if (!(Test-Path $TempDir)) {
    $null = mkdir $TempDir
}

# Determine if the destination already contains binaries
$extensions = @('.dll', '.exe', '.sys')
$existingBinaries = (Get-ChildItem -recurse $OutDir | Where-Object Extension -In $extensions).Count -gt 0

# Download the appinstaller to find the current uri for the msixbundle
Invoke-WebRequest https://aka.ms/ttd/download -OutFile $TempDir\ttd.appinstaller

# Download the msixbundle
$msixBundleUri = ([xml](Get-Content $TempDir\ttd.appinstaller)).AppInstaller.MainBundle.Uri

if ($PSVersionTable.PSVersion.Major -lt 6) {
    # This is a workaround to get better performance on older versions of PowerShell
    $ProgressPreference = 'SilentlyContinue'
}

# Download the msixbundle (but name as zip for older versions of Expand-Archive)
Invoke-WebRequest $msixBundleUri -OutFile $TempDir\ttd.zip

# Extract the 3 msix files (plus other files)
Expand-Archive -DestinationPath $TempDir\UnzippedBundle $TempDir\ttd.zip -Force

# Expand the build you want - also renaming the msix to zip for Windows PowerShell
$fileName = switch ($Arch) {
    "x64"   { "TTD-x64"   }
    "x86"   { "TTD-x86"   }
    "arm64" { "TTD-ARM64" }
}

# Rename msix (for older versions of Expand-Archive) and extract the debugger
Rename-Item "$TempDir\UnzippedBundle\$fileName.msix" "$fileName.zip"
Expand-Archive -DestinationPath "$OutDir" "$TempDir\UnzippedBundle\$fileName.zip"

# Delete the temp directory
Remove-Item $TempDir -Recurse -Force

# Remove unnecessary files, if it is safe to do so
if (-not $existingBinaries) {
    Get-ChildItem -Recurse -File $OutDir |
        Where-Object Extension -NotIn $extensions |
        Remove-Item -Force

Remove-Item -Recurse -Force (Join-Path $OutDir "AppxMetadata")
} else {
    Write-Host "Detected pre-existing binaries in '$OutDir' so did not remove any files from TTD package."
}
