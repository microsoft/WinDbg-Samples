param(
    [string]$OutputDir = "$PSScriptRoot\\out"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"


Import-Module -Force -Name .\SourceControl.psm1

<#
    .SYNOPSIS
    A Powershell function that interfaces with the source link compiler flag.

    .PARAMETER sourcesDirectoryPath
    This refers to the local directory path of the sources we reference (in our lib/obj files) for which we want 
    to generate a source link for with git.  

    .PARAMETER SourceLinkOutputPath
    This defines the path where the json config output file should be created/obtained.

    .NOTES
    This script interfaces with the /sourcelink compiler flag. It produces any inputs that
    the /sourcelink option may require such as:

    JSON configuration file: A json configuration containing a simple mapping of local file
    path to URL where the source file can be retrieved. A debugger would retrieve the original
    file path of the current location from the PDB, look this path up in the Source Link map,
    and use the resulting URL to download the source file. 

    Additional Info: When we compile/build a lib with debug info (/Zi) at a path P, the object files references sources 
    as relative to P.  When P is no longer valid (i.e. because we handed our .lib to someone else), compiling with this 
    sourceLink will alias all paths to P (or $sourcesDirectoryPath
    in this case) to the git repo.
#>
Function GenerateSourceLinkJsonConfig($sourcesDirectoryPath, $sourceLinkOutputPath)
{
    $sourceFilePath = "*"
    $headCommitId = Get-RepositoryHeadCommitId
    $repository = Get-Repository
    $owner = Get-Repository-Owner

    if (!$sourcesDirectoryPath -or !$headCommitId -or !$repository)
    {
        Write-Error "One or more values are null or empty. [Repository Directory=$repositoryDirectory;Head Commit ID=$headCommitId;Repository=$repository]"
    }

    $url = "https://raw.githubusercontent.com/${owner}/${repository}/${headCommitId}/${sourceFilePath}"

    $jsonObject = @{
        "documents" = [ordered]@{
            "$sourcesDirectoryPath\*" = $url;
            }
    }

    ConvertTo-Json -InputObject $jsonObject  | Out-File -FilePath $sourceLinkOutputPath -Encoding utf8

    Write-Host "Generated the following source link json config file: "
    Get-Content $sourceLinkOutputPath
}

## Start Script Here

$srcDir = $PSScriptRoot
GenerateSourceLinkJsonConfig $srcDir "$OutputDir\sourcelink.json"