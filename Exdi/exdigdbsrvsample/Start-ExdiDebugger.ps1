<#
.Synopsis
    Installs and launches exdi debugger (automating xml file editing)

.Description
    This script will install ExdiGdbSrvSample.dll if required, configure the xml settings
    files, check for running dllhost.exe processes, and launch the debugger to connect to
    an already running gdb server hardware debugging target.

.Parameter ExdiTarget
    Type of target to connect to. This corresponds to a specific section in the settings xml file

.Parameter HostName
    IP address or hostname of the computer hosting the gdb server session (defaults to "LocalHost")

.Parameter GdbPort
    Port that the gdb server is listening on.

.Parameter Architecture
    Architecture of the hardware debugging target (this parameter also implies the ArchitectureFamily
    parameter in the xml settings file)

.Parameter ExdiDropPath
    Location of the ExdiGdbSrvSample.dll, exdiConfigData.xml, and systemregisters.xml files. These will
    only be copied if ExdiGdbSrvSample.dll is not installed or is installed incorrectly.

.Parameter ExtraDebuggerArgs
    Extra arguments to pass on the debugger command line

.Parameter PreNTAppDebugging
    Changes the value of the heuristicScanSize from 0xfffe (best for NT) to 0xffe (for pre-NT Apps)

.Parameter DontTryDllHostCleanup
    Checking for existing running instances of ExdiGdbSrvSample.dll in dllhost.exe requires elevation.
    Providing this switch will allow the script to run without elevation (although the debugger may not
    function correctly).

.Example
    >---------------- MSFT External usage (first run) ------------<
    .\Start-ExdiDebugger.ps1 -ExdiTarget "QEMU" -GdbPort 1234 -Architecture x64 -ExdiDropPath "C:\path\to\built\exdi\files"

.Example
    >---------------- MSFT Internal usage (first run) ------------<
    .\Start-ExdiDebugger.ps1 -ExdiTarget "QEMU" -GdbPort 1234 -Architecture x64 -ExdiDropPath "\\dbg\privates\daily\latest\uncompressed\amd64\exdi"

.Example
    PS>.\Start-ExdiDebugger.ps1 -ExdiTarget "QEMU" -GdbPort 1234 -Architecture x64
#>

[CmdletBinding()]
param
(
    [ValidateSet("QEMU")]
    [string]
    $ExdiTarget = "QEMU",

    [string]
    $HostName = "LocalHost",

    [Parameter(Mandatory=$true)]
    [Int]
    $GdbPort,

    [Parameter(Mandatory=$true)]
    [string]
    [ValidateSet("x86", "x64", "arm64")]
    $Architecture,

    [string]
    $ExdiDropPath,

    [string]
    $DebuggerPath,

    [string[]]
    $ExtraDebuggerArgs = @(),

    [switch]
    $PreNTAppDebugging,

    [switch]
    $DontTryDllHostCleanup
)

$ErrorActionPreference = "Stop"

#region Functions

Function Test-Admin
{
    ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")
}

Function Find-PathToWindbgX
{
    $InternalWindbgXPath = "$env:LOCALAPPDATA\DBG\UI\WindbgX.exe"
    $ExternalWindbgXPath = "$env:LOCALAPPDATA\Microsoft\WindowsApps\WinDbgX.exe"

    if (Test-Path $InternalWindbgXPath -PathType Leaf)
    {
        return $InternalWindbgXPath
    }
    elseif (Test-Path $ExternalWindbgXPath -PathType Leaf)
    {
        return $ExternalWindbgXPath
    }
}

Function Test-ParameterValidation
{
    $CommandName = $PSCmdlet.MyInvocation.InvocationName
    $ParameterList = (Get-Command -Name $CommandName).Parameters

    foreach ($Parameter in $ParameterList) {
        Get-Variable -Name $Parameter.Values.Name -ErrorAction SilentlyContinue | Out-String | Write-Verbose
    }

    if (-not $DebuggerPath)
    {
        throw "WindbgX is not installed"
    }
    elseif (-not (Test-Path $DebuggerPath -PathType Leaf))
    {
        throw "DebuggerPath param ($DebuggerPath) does not point to a debugger."
    }

    # Searching for loaded instances of ExdiGdbSrvSample.dll in dllhost.exe requires elevation
    if (-not $DontTryDllHostCleanup -and
        -not $(Test-Admin))
    {
        throw "Searching for loaded instances of ExdiGdbSrvSample.dll in dllhost.exe requires elevation. Run with the -DontTryDllHostCleanup parameter to skip this check (debugger session init may fail)."
    }
}

Function Get-ExdiInstallPath
{
    Get-ItemPropertyValue -Path "Registry::HKEY_CLASSES_ROOT\CLSID\{29f9906e-9dbe-4d4b-b0fb-6acf7fb6d014}\InProcServer32" -Name "(default)" -ErrorAction SilentlyContinue
}

Function Test-ExdiServerInstalled
{
    # Check registration of exdi server class
    if ($(Get-ExdiInstallPath) -ne $null -and $(Test-Path "$(Get-ExdiInstallPath)"))
    {
        Write-Verbose "Exdi server is installed. Checking installation..."
        $ExdiInstallDir = [System.IO.Path]::GetDirectoryName($(Get-ExdiInstallPath))
        if (-not (Test-Path $ExdiInstallDir))
        {
            Write-Host "Currently Registered exdi server does not exist. Reinstalling..."
            return $false
        }
        elseif (-not ((Test-Path "$ExdiInstallDir\exdiConfigData.xml") -and (Test-Path "$ExdiInstallDir\systemregisters.xml")))
        {
            Write-Host "Currently Registered exdi server does not have required xml settings files. Reinstalling..."
            return $false
        }
        else
        {
            Write-Verbose "Exdi server is insalled correctly. Skipping installation..."
            return $true
        }
    }
    else
    {
        Write-Host "Exdi server is not installed. Installing..."
        return $false
    }
}

Function Install-ExdiServer
{
    [CmdletBinding()]
    param
    (
        [string] $InstallFrom,
        [string] $InstallTo
    )
    
    if (-not $(Test-Admin))
    {
        throw "Script needs to be run as an Admin to install exdi software."
    }

    New-Item -ItemType Directory $InstallTo -ErrorAction SilentlyContinue | Write-Verbose
    Copy-Item -Path "$InstallFrom\ExdiGdbSrvSample.dll" -Destination $InstallTo -ErrorAction stop | Write-Verbose
    Copy-Item -Path "$InstallFrom\exdiConfigData.xml" -Destination $InstallTo -ErrorAction stop | Write-Verbose
    Copy-Item -Path "$InstallFrom\systemregisters.xml" -Destination $InstallTo -ErrorAction stop | Write-Verbose
    regsvr32 /s "$InstallTo\ExdiGdbSrvSample.dll"

    if ($(Get-ExdiInstallPath) -eq $null)
    {
        throw "Unable to install exdi server"
    }
}

Function Edit-ExdiConfigFile
{
    [CmdletBinding()]
    param
    (
        [string] $ExdiFilePath,
        [string] $ExdiTargetType,
        [PSCustomObject[]] $XmlSettingPathValueList
    )
    
    # Edit exdiConfigData.xml
    [xml]$exdiConfigXml = Get-Content "$ExdiFilePath"

    # Set current target
    $exdiConfigXml.ExdiTargets.CurrentTarget = $ExdiTarget

    # set HostNameAndPort
    $ExdiTargetXmlNode = $exdiConfigXml.SelectSingleNode("//ExdiTargets/ExdiTarget[@Name='$ExdiTarget']/ExdiGdbServerConfigData")

    foreach ($XmlSettingPathValue in $XmlSettingPathValueList)
    {
        Write-Verbose "Processing $XmlSettingPathValue"
        if ($XmlSettingPathValue.Value -eq $null)
        {
            continue
        }

        $PathParts = $XmlSettingPathValue.Path.Split(".")
        $curNode = $ExdiTargetXmlNode
        if ($PathParts.Count -gt 1)
        {
            foreach ($PathPart in $PathParts[0..($PathParts.Count-2)])
            {
                Write-Verbose $PathPart
                $curNode = $curNode.($PathPart)
            }
        }
        $curNode.($PathParts[-1]) = $XmlSettingPathValue.Value
    }

    $exdiConfigXml.Save("$ExdiFilePath")
}

Function Stop-ExdiContainingDllHosts
{
    $DllHostPids = Get-Process dllhost | ForEach-Object { $_.Id }
    foreach ($DllHostPid in $DllHostPids)
    {
        $DllHostExdiDlls = Get-Process -Id $DllHostPid -Module | Where-Object { $_.FileName -like "*ExdiGdbSrvSample.dll" }
        if ($DllHostExdiDlls.Count -ne 0)
        {
            Write-Verbose "Killing dllhost.exe with pid $DllHostPid (Contained instance of ExdiGdbSrvSample.dll)"
            Stop-Process -Id $DllHostPid -Force
        }
    }
}

#endregion

#region Script

# Apply defaults for $DebuggerPath before Parameter validation
if (-not $DebuggerPath)
{
    $DebuggerPath = Find-PathToWindbgX
}

Test-ParameterValidation

# look clean up dllhost.exe early since it can hold a lock on files which
# need to be overwritten
if (-not $DontTryDllHostCleanup)
{
    Stop-ExdiContainingDllHosts
}

if (-not $(Test-ExdiServerInstalled))
{
    if (-not $ExdiDropPath)
    {
        throw "ExdiServer is not installed and -ExdiDropPath is not valid"
    }

    $ExdiInstallDir = Join-Path -Path "$([System.IO.Path]::GetDirectoryName($DebuggerPath))" -ChildPath "exdi"
    Install-ExdiServer -InstallFrom "$ExdiDropPath" -InstallTo "$ExdiInstallDir"
}

$SystemRegistersFilepath = Join-Path -Path "$([System.IO.Path]::GetDirectoryName($(Get-ExdiInstallPath)))" -ChildPath "systemregisters.xml"
$ExdiConfigFilepath      = Join-Path -Path "$([System.IO.Path]::GetDirectoryName($(Get-ExdiInstallPath)))" -ChildPath "exdiConfigData.xml"

# Calculate implied parameters
$HeuristicScanSize = if ($PreNTAppDebugging) { "0xffe" } else { "0xfffe" }
$ArchitectureFamily = switch($Architecture)
{
    x64   { "ProcessorFamilyx64" }
    x86   { "ProcessorFamilyx86" }
    arm64 { "ProcessorFamilyARM64" }
}

# Path is evaluated relative to the relevant ExdiTarget's ExdiGdbServerConfigData node in the xml schema
$SettingsToChange = @(
    [pscustomobject]@{ Path = 'GdbServerConnectionParameters.Value.HostNameAndPort' ; Value = "${HostName}:$GdbPort" },
    [pscustomobject]@{ Path = 'ExdiGdbServerTargetData.targetArchitecture'          ; Value = "$Architecture" },
    [pscustomobject]@{ Path = 'ExdiGdbServerTargetData.targetFamily'                ; Value = "$ArchitectureFamily" },
    [pscustomobject]@{ Path = 'ExdiGdbServerTargetData.heuristicScanSize'           ; Value = "$HeuristicScanSize" },
    [pscustomobject]@{ Path = 'displayCommPackets'                                  ; value = "no" }
)
Edit-ExdiConfigFile -ExdiFilePath "$ExdiConfigFilepath" -ExdiTargetType "$ExdiTarget" -XmlSettingPathValueList $SettingsToChange

# Set env vars for debugger
[System.Environment]::SetEnvironmentVariable('EXDI_GDBSRV_XML_CONFIG_FILE',"$ExdiConfigFilepath")
[System.Environment]::SetEnvironmentVariable('EXDI_SYSTEM_REGISTERS_MAP_XML_FILE',"$SystemRegistersFilepath")

$DebuggerArgs = @("-v", "-kx exdi:CLSID={29f9906e-9dbe-4d4b-b0fb-6acf7fb6d014},Kd=Guess,DataBreaks=Exdi")
Write-Verbose "DebuggerPath = $DebuggerPath"
Start-Process -FilePath "$DebuggerPath" -ArgumentList ($DebuggerArgs + $ExtraDebuggerArgs)

#endregion