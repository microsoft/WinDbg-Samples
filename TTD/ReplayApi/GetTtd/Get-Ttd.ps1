param(
    $OutDir = ".\TTDDownload",
    [ValidateSet("x64", "x86", "arm64")]
    $Arch = "x64",
    $MsixBundle = ""
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

$MsixBundleZip = "$TempDir\ttd.zip"

$ttdReplayFiles = @(
    "TTDReplay.dll",
    "TTDReplayCPU.dll"
)

if ($MsixBundle -eq "") {
    Write-Host "Downloading TTD from https://aka.ms/ttd/download"

    # Download the appinstaller to find the current uri for the msixbundle
    Invoke-WebRequest https://aka.ms/ttd/download -OutFile $TempDir\ttd.appinstaller

    # Download the msixbundle
    $msixBundleUri = ([xml](Get-Content $TempDir\ttd.appinstaller)).AppInstaller.MainBundle.Uri

    if ($PSVersionTable.PSVersion.Major -lt 6) {
        # This is a workaround to get better performance on older versions of PowerShell
        $ProgressPreference = 'SilentlyContinue'
    }

    # Download the msixbundle (but name as zip for older versions of Expand-Archive)
    Invoke-WebRequest $msixBundleUri -OutFile $MsixBundleZip
} else {
    # Copy and rename the bundle so it can be expanded.
    Copy-Item $MsixBundle -Destination $MsixBundleZip
}

# Extract the 3 msix files (plus other files)
Expand-Archive -DestinationPath $TempDir\UnzippedBundle $MsixBundleZip -Force

foreach ($archName in @("x64", "x86", "ARM64")) {
    $msixName = "TTD-$archName"
    $msixOutDir = "$OutDir\$archName"

    if (!(Test-Path $msixOutDir)) {
        $null = mkdir $msixOutDir
    }

    # Rename msix (for older versions of Expand-Archive) and extract TTD.
    Rename-Item "$TempDir\UnzippedBundle\$msixName.msix" "$msixName.zip"
    Expand-Archive -DestinationPath $msixOutDir "$TempDir\UnzippedBundle\$msixName.zip" -Force

    # Remove unnecessary files
    Get-ChildItem -Recurse -File $msixOutDir  -Exclude $ttdReplayFiles |
        Remove-Item -Force

    # Remove subdirectories
    Get-ChildItem -Directory $msixOutDir | Remove-Item -Recurse -Force
}

# Delete the temp directory
Remove-Item $TempDir -Recurse -Force
