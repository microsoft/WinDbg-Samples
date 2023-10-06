<#
    Helper functions for interacting with the git repo information
#>
Function Get-RepositoryCommitId($branchName)
{
    $command = "git --git-dir .\.git rev-parse $branchName"

    [array]$gitRevParseHeadOutput = @(Invoke-Expression -Command $command)
    $gitRevParseHeadOutput | ForEach-Object { $_.Trim() } | Out-Null

    # Example: d04eef712a11b669db896ff208556c06d883652c
    [string]$output = If ($gitRevParseHeadOutput.Count -gt 0) { $gitRevParseHeadOutput[0] } Else { "" }

    return $output
}

Function Get-RepositoryHeadCommitId()
{
    return Get-RepositoryCommitId("HEAD");
}

Function Get-RepositoryOriginUrl()
{
    $command = "git --git-dir $ENV:BASEDIR\\.git remote get-url origin"

    [string]$windowsRepositoryURL = Invoke-Expression -Command $command
    $windowsRepositoryURL.Trim() | Out-Null

    # On official build machines, the remote URL will have a special syntax, such as:
    #     "https://microsoft.visualstudio.com/OS/_git/_full/os1test"
    # This is a special URL used when cloning that bypasses AzDO's limited refs.
    # This form of the URL isn't used outside of the clone operation, so we have to
    # remove the "_full/" path segment to make the URL generally applicable.
    $windowsRepositoryURL -replace '_full','' | Out-Null

    return $windowsRepositoryURL
}

Function Get-Repository()
{
    $command = "git remote get-url origin"

    [string]$repository = Split-Path -Leaf -Path (Invoke-Expression -Command $command)
    $repository.Trim() | Out-Null

    return $repository
}

Function Get-Repository-Owner()
{
    $command = "git config --get remote.origin.url"
    $repository = Invoke-Expression -Command $command
    [string]$owner = ForEach-Object {
        if ($repository -match 'github\.com[:/](.*?)\/') {
            $matches[1]
        }
    }
    $owner.Trim() | Out-Null

    return $owner
}