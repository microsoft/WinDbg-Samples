# EXDI Gdbserver – Getting Started

EXDI is an interface that allows extending WinDbg by adding support for hardware debuggers (e.g. JTAG-based, or GdbServer-JTAG based).  
The diagram below illustrates the role of EXDI-GdbServer:

![](./Windbg_Exdi_interface.png?raw=true)

This document will describe the required steps to establish a GdbServer RSP session between the ExdiGdbSrv.dll (GDB server client) and various Front End GdbServer HW debugger implementations:

1. QEMU GDB Server Front End.

- Debug QEMU ARM64 and x64 Windows VMs on Ubuntu server

- Debug QEMU x64 windows VM on Windows

2. Trace32 GDB server Front End.

- The T32-GdbServer multi-core Front-End (legacy method).
This method establishes separated GdbServer RSP sessions (over multi-socket connections) for each processor core supported by the target, so each GdbServer T32 instance controls a separate core via the T32–JTAG interface. 

- The T32-GdbServer one core Front-End (recommended method)
This method established one GdbServer RSP session (over one socket connection) for all core supported by the target, so there will be only one T32 debugger instance launched for the JTAG-T32 interface to control the target HW device.

3. VMWare workstation GdbServer.
This method establishes only one GdbServer RSP session with the VMware GdbServer workstation software. This GdbServer controls the target hardware.

## EXDI Gdbserver with QEMU GdbServer Front-End

This document walks through setting up a Windows VM for hardware debugging via QEMU GDB server.
Before enabling QEMU over WinDbg-Exdi Gdb server, please complete the pre-requisites below. QEMU over WinDbg today is supported on ARM64 and X64 VMs.

QEMU is a generic and open source machine emulator and virtualizer with incredible performance via dynamic translation. When QEMU is used as a machine emulator - it can run OS’s and programs made for one machine 
(e.g. an ARM64/x64 image) on a different machine (e.g. your x64 PC).

QEMU can use other hypervisors like KVM to use CPU extensions (HVM) for virtualization. When QEMU is used as a virtualizer, QEMU achieves near native performances by executing the guest code directly on the host CPU. QEMU can take advantage of OS hypervisor features to offload CPU and MMU emulation to real hardware. (Recent versions support both the KVM and Hyper-V hypervisors.)

## Pre-Requisites

Windbg-ExdiGdb server solution supports debugging QEMU ARM64 and x64 Windows VMs.

Important note: ARM64 QEMU VM runs slower on Windows than on Ubuntu Server, so it’s recommended to use Ubuntu as a QEMU Host to run ARM64 Windows VM, but x64 QEMU Windows VMs run fine on both VM host OS (Windows and Ubuntu server).

Option 1: Installing QEMU on Ubuntu Server 
1. Install Ubuntu server on x64 machine (recommended Ubuntu version 20.10)
•	To install QEMU on Ubuntu server please visit either location to install the required packages:
- Visit: https://wiki.archlinux.org/title/QEMU
 
2. Prepare a Windows ARM64/x64 VM image to run on QEMU:

• Locate or create a Windows VHDX file. It’s required that the build have symbols indexed on the public symbol server.

•Prepare a script to setup the image for these steps.
- Get the QEMU para-virtualized drivers added to the image
	- Download this driver: https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/latest-virtio
- Unpack the downloaded drives and add them to the VHDX Windows image.
- Run the script to inject the drivers and prepare the required BCD settings to be added to the image (ensure to specify the correct VHDX image file in the conf_image.ps1). Please see below a sample of the script to inject drivers and enable BCD KDNET settings
- Convert the VHDX image with drivers injected to qcow2 format by:
e.g. c:\Program Files\qemu>qemu-img convert -c -p -O qcow2 Windowsfile.vhdx fileQEMU.qcow2

3. Create a Linux script file to launch the Windows VM on QEMU.
•	Once you prepared Windows VHDX image (and added the set of paravirtualized drivers and converted to qcow2 format) on the Ubuntu server.
- - Get the QEMU VM UEFI from https://github.com/clearlinux/common/blob/master/OVMF.fd (ensure to get the correct architecture ARM64/x64 UEFI file)
•	Update the the launching VM script to point to the locations where the image and UEFI will be located.
•	Please see below samples of Linux & Windows launching QEMU Windows VM scripts

Option 2: Installing QEMU on Windows 
- There is documentation on installing QEMU on Windows through the following sources:
- QEMU for Windows – Installers (64 bit) (weilnetz.de) - https://qemu.weilnetz.de/w64/

## Steps to enable debugging QEMU VM via Windbg-ExdiGdbServer

1. Setup Windbg-Exdi-GDBsrv solution

Install the Windows debugger package accordingly to the architecture of your Host machine.

Grab the corresponding ExdiGdbSrv.dll binary (EXDI COM server client) from git clone WinDbg-Samples/Exdi/exdigdbsrv at master · microsoft/WinDbg-Samples · GitHub

Build the VS solution (ExdiGdbSrv.sln) according to the architecture of your Host Debugger installation.

Once you copy the EXDI com server (ExdiGdbSrv.dll) to the Host machine, into the directory containing your debugger (e.g. c:\Debuggers), then register the server by running:
 
o	regsvr32 c:\Debuggers\ExdiGdbSrv.dll in an Administrator command prompt.

o	Note: This is done once, but if you change the location of the ExdiGdbSrv.dll, then you need to register the COM server again.

- Copy the exdiConfigData.xml and systemregisters.xml files to your Host debugger machine (where the debugger is installed, e.g. c:\Debuggers).

o	These files should be present on WinDbg-Samples/Exdi/exdigdbsrv/GdbSrvControllerLib at master · microsoft/WinDbg-Samples · GitHub\exdiConfigData.xml WinDbg-Samples/Exdi/exdigdbsrv/

- GdbSrvControllerLib at master · microsoft/WinDbg-Samples · GitHub\systemregisters.xml

- Open a command prompt and set the following environment variables:
o	set EXDI_GDBSRV_XML_CONFIG_FILE=c:\debuggers\exdiConfigData.xml

o	set EXDI_SYSTEM_REGISTERS_MAP_XML_FILE=c:\debuggers\systemregisters.xml

Important notes about the above commands:
-  EXDI_GDBSRV_XML_CONFIG_FILE – will contain the full path to the Exdi xml configuration file (see below details about this file).
-  EXDI_SYSTEM_REGISTERS_MAP_XML_FILE – will contain the full path to the Exdi xml system register map file (see below details).
-  Please ensure that the path specified is available from the location of the ExdiGdbSrvSample.dll

2. Start the GDB server on the guest QEMU session

### QEMU on Ubuntu Server OS

- Within your session QEMU on Ubuntu Server

- Get the IP address of the QEMU machine by running in a command prompt: hostname -I
o   Output will be the IP address which you will need when the Host debugger will be launched in the exdiConfigDat.xml file

- Ensure that the Ubuntu firewall allows the Host debugger machine to access the target by running:
`sudo ufw allow from [IP.address Host debugger machine] to any port [port number]`

Verify the new rule has been added by running: sudo ufw status verbose

Launch the linux VM script (x64/ARM64 QEMU Windows VMs are supported)

![](./Launch_VM_script_on_Ubuntu.png?raw=true)

- •	Once the Windows VM is launched in the QEMU environment, launch the GDB server by going to the View->compatmonitor0
o	Important note: you also can launch the GDB server from the VM script.

![](./Select_GDBServer_screen_on_QEMU_Ubuntu.png?raw=true)
 
On the prompt - type: gdbserver

![](./launch_gdbServer_on_QEMU_Ubuntu.png?raw=true)

- If the gdbserver started fine, then you will see the port number where the GDB server will be listening, and you will need to use this port to setup the host debugger (IP:Port pair) in the exdiConfigData.xml)
o   Tip: You can switch between VM/Gdbserver windows by pressing Ctrl+Al+1/Ctrl+Alt+3

![](./Windows_QEMU_VM_on_Ubuntus.png?raw=true)

### QEMU on Windows OS

Ensure to get the Windows IP address (if the debugger host session won't be located at the same Windows machine as QEMU VM).

Enable windbg through the Windows firewall for the host debugger machine (if the debugger host session won't be located at the same Windows machine as QEMU VM).

Launch the Windows script to run x64 QEMU VM (ARM64 QEMU VM is not recommended to be used to run on Windows, it's very slow) by going to the QEMU installation directory and pointing to the Windows script
o	e.g. 
>`c:\Program Files\qemu\d:\QEMU_x64_scripts\start_bk_amd64_client_hp.cmd`

![](./Windows_QEMU_VM_on_Windows.png?raw=true)

Once the Windows VM is launched in the QEMU environment, launch the GDB server by going to the View->compatmonitor0
o   Important note: you also can launch the GDB server from the VM script, so once gdbserver is launched as a front end for the QEMU VM

Type gdbserver to launch the front end GDB server on QEMU.

![](./Launch_QEMU_GDBserver_On_Windows.png?raw=true)

- If the GDB server started fine, then you will see the port number where the GDB server will be listening, and you will need to use this port to setup the host debugger (IP:Port pair) in the 
exdiConfigData.xml). If your Host debugger is located at the same machine that host the QEMU guest, then the Localhost identifier will be used in the exdiconfigdata.xml as IP:Port pair (e.g. LocalHost:Port:1234)

o   Tip: You can switch between VM/GDB Server window by pressing Ctrl+Al+1/Ctrl+Alt+3

![](./QEMU_Windows_VM_desktop.png?raw=true)

3. Launch WinDbg on the Host debugger computer
- Ensure that Host debugger machine and Target QEMU machine are reachable by the network:

o   Example - within command prompt: On Host: ping <target IP address QEMU> and On Target: ping <host debugger machine IP address>

- Set the current target name attribute (CurrentTarget) value to “QEMU” in the ExdiConfigData.xml file.

![](./Exdi_Targets_Xml_Element.png?raw=true)

- Set the target QEMU IP <address> : Port <number> where GDB server is listening by:

o   Finding the QEMU component Tag element in the exdiCondifgData.xml.

![](./Exdi_Target_Name_HW_debugger_Element.png?raw=true)


- Set the IP:Port number (LocalHost if Debugger runs on the same Host as QEMU VM) for the QEMU GDB server by:

![](./Exdi_Target_IP_Port_XML_Element.png?raw=true)

- Save the changes to the exdiConfigdata.xml file that is located at path specified by EXDI_GDBSRV_XML_CONFIG_FILE environment variable.

- Launch the windbg session via exdi interface at the same command prompt where you set the environment variables (EXDI_GDBSRV_XML_CONFIG_FILE and EXDI_SYSTEM_REGISTERS_MAP_XML_FILE) by:

o   Example: 
- >c:\Debuggers\windbg.exe -v -kx exdi:CLSID={29f9906e-9dbe-4d4b-b0fb-6acf7fb6d014},Kd=Guess,DataBreaks=Exdi

- If the QEMU GDB server target is reachable and the host has access to the system application .pdb file (e.g. for NT OS: ntkrnlmp.pdb, for boot manager: bootmgfw.pdb, etc.), then the host debugger should
automatically break-in into the target and the debugger session will be up and running.

![](./windbg_session_QEMU.png?raw=true)


## EXDI Gdbserver with T32 GdbServer Front-End

**Steps to start an ExdiGdbSrv.dll session with T32 Back-End**

Before starting to do anything with this example, please follow the preparation steps below:

1. Install the amd64 Debugging Tools for Windows.

2. Build the ExdiGdbSrv.sln solution and register ExdiGdbSrv.dll produced by the build by running ‘regsvr32 ExdiGdbSrv.dll’ as Administrator.

3.	Install T32 Lauterbach software with the update for T32–GdbServer Back-End. See this website for information about the Lauterbach software:
http://www.lauterbach.com/

4.	Create a script to start T32. Contact Microsoft to see if the t32start.cmd script sample is available.
- 4.1	Add APPS_CORE entries to the USB setting tab (if you have four cores, then you have to add four APPS_COREs entries, and enable four GDB Port entries, see below the pic 1).
- 4.2	Modify the Interface setting tab by enabling the GDB Port and disable the API port and Intercom Port for each application core that will be debugged via the Exdi component.
- Use Port:  Set it to ‘y’.  This enables working with the GdbServer Back-End , so the Exdi component will communicate with each T32 core instance via the Gdb RSP protocol.
- 4.3	Port start value:  This is the TCP port where the T32 GdbServer Back-End port number will 
- listen for incoming connections. This port number should match with the port specified in the GdbServer connection string (please see below the xml configuration file).
- 4.4	Protocol: Enable TCP port.
- 4.5	Max UDP packet size: This is the TCP port maximum packet size.
- 4.6	Use Auto Increment Port: Set it to ‘no’.

After defining each application core tab, then you can start each T32-GdbServer instances and attach to the target via the JTAG (please see point 7).

![](./T32start_ts2.png?raw=true)

Pic 1. T32start.cmd script for launching APPS_COREs and defining GDB ports.

5.	Set the environment variable EXDI_GDBSRV_XML_CONFIG_FILE that will contain the full path to the exdi xml configuration file (please see below the exdiConfigData.xml tags and attributes).
- >`set EXDI_GDBSRV_XML_CONFIG_FILE=<full path to exdiConfigData.xml>`.
- Please ensure that the path specified is available from the location of the ExdiGdbSrv.dll.

6.	Start the RPM tab.
- You need to launch and attach (press the radio button ‘Attach’) to the T32 instance assigned to the RPM processor before any other APPS_COREs.


7.	Start each T32 core instances by clicking each APP_CORE<n> tab.
- 7.1	Attach to each T32 core instance by pressing the radio button ‘Attach’ or entering ‘sys.mode.attach’ in the T32 command prompt.

- 7.2	Break (enter ‘break’) each T32 instance (please see pic 2.).

![](./T32_system_state.png?raw=true)

Pic 2 Attach and break commands in T32.

- •	You will need to enter these commands on every T32 instance, and the commands have to succeed (the status should be ‘stopped’).
- •	If the break command fails, then you will see a green color ‘running’ status in the status bar, so you will need to launch the T32 sequence again as the target is not in the break state and the debugger
(windbg. exe-ExdiGdbSrv.dll) won’t be able to connect to it.

8.	Start the windbg session by specifying the following command line:
- *Windbg.exe  -v -kx exdi:CLSID={29f9906e-9dbe-4d4b-b0fb-6acf7fb6d014}, Kd=Guess, DataBreaks=Exdi*

- -v:- 	-  verbose session,
- -kx:   	-  Exdi-kernel session,
- CLSID:	-  Class ID assigned to the LiveExdiGdbSrvServer (it’s defined in theExdiGdbSrv.idl file),
 Kd=Guess  Dbgeng will use the heuristic algorithm for finding the KdVersionBlock address,
- ForceX86-  Forces dbgeng to use the IeXdiX86Context3 interface for getting/setting The CPU context,
- DataBreaks   Allow using data breakpoints.
- 
- Other options:
- Inproc	-    Allow using an inproc Exdi-Server.

![](./windbg_session_T32.png?raw=true)

9.	At this point, the Exdi-GdbServer session is up and running, so you can get symbols (.reload /f) and start your debugging session with the GdbServer.
- •	If the .reload /f command failed, then it is probably due to the debugger (dbgeng.dll) could not find the location of the symbols for the running target code, so you will need to specify manually the
location of the ntkrnlmp.exe pdb file. The dbgeng.dll uses a heuristic algorithm to find the location of the nt module pdb file based on the location of the pc address at the time that the break command occurred. 
- •	If you have a local build, then you can specify the location of the symbols by setting the _NT_SYMBOL_PATH environment variable.


## Troubleshooting

![](./QEMU_TroubleShooting.png?raw=true)


### Exdi xml configuration file

There are two required xml files that are consumed by the EXDI GDB COM server (ExdiGdbSrv.dll).

1. exdiConfigData.xml: This file contains the main configuration data that is required by the GDB server client to establish a successfully GDB session with the HW debugger GDB server target, so the GDB server client won’t run if the file location is *not* set by the EXDI_GDBSRV_XML_CONFIG_FILE environment variable.
- Each xml tag allows configuring specific set of the GDB server functionality. To personalize the experinece - see below all the attributes you can modify in the XML and a sample XML below.

2. Systemregister.xml: This file contains a mapping between system registers and theirs access code. This is needed because the access code is *not* provided by the GDB server in the xml file, and the debugger accesses each system register via the access code. If the file is not set via the environment variable EXDI_SYSTEM_REGISTERS_MAP_XML_FILE , then the ExdiGdbSrv.dll will continue working, but the debugger won’t be able to access any system register via rdmsr/wrmsr commands. The list of these registers should be supported by the GDB server HW debugger (the specific system register name should be present in the list of registers that is sent in the system xml file).

## Tags and attributes

- ExdiTargets: Specifies which specific GDB server target configuration will be used by the ExdiGgbSrv.dll to establish the GDB connection with the GDB server target, since the exdiConfigData.xml file includes
-  all GDB server supported currently by the ExdiGdbSrv.dll (this file MUST be filled before using the ExdiGdbSrv.dll with a particular GDB server).
- •	CurrentTarget: Specifies the name of the GDB server target (e.g. this attribute value should match with the name value of one of the `<ExdiTarget Name=` tags included by the exdiConfigData.xml file.
- •	ExdiTarget: this is the start tag for all configuration data that is included by each GDB server target component.
- •	Name: Specifies the name of the GDB server (e.g. QEMU, BMC-OpenOCD, Tarce32, VMWare).
- •	agentNamePacket: This is the name of the GDB client as it is recognized by the GDB server HW debugger. This can be used by the GDB server HW debugger to configure itself for specific GDB clients (e.g.
 Trace32 GDB server requires the ExdiGdbSrv.dll to send “QMS.windbg” name to identify the windbg-GDB client and then enable customized GDB memory packets only supported for MS GDB server client (exdiGdbSrv.dll).
- •	ExdiGdbServerConfigData: Specifies the ExdiGdbSrv.dll component related configuration parameters.
- •	uuid: specifies the UUI of the ExdiGdbSrv.dll component.
- •	displayCommPackets: Flag if ‘yes’, then we will display the RSP protocol communication characters in the command log window. If ‘no’, then we display just the request-response pair text.
- •	enableThrowExceptionOnMemoryErrors: This attribute will be checked by the GDB server client when there is a GDB error response packet (E0x) to determine if the client should throw an exception and stop
- reading memory.
- •	qSupportedPacket: This allows configuring the GDB client to request which xml register architecture file should be sent by the GDB server HW debugger following the xml target description file (basically, the
  client will inform the GDB server which architectures are supported by the client, currently, the client does support the x64 architecture).
- •	ExdiGdbServerTargetData:  Specifies the parameters related to the hardware Target that is debugged by the GdbServer session.
- •	targetArchitecture: String containing the target hardware architecture. Possible values: X86, X64, ARM, ARM64. Currently, the exdiGdbSrv.dll supports only X86 and ARM.
- •	targetFamily: String containing the target hardware family. Possible values: ProcessorFamilyX86, ProcessorFamilyX64, ProcessorFamilyARM, ProcessorFamilyARM64.
- •	numberOfCores: Number of processor cores that the target support. This parameter will be validated when we use a multi-Gdbserver session (T32-GdbServer session). The below ‘MultiCoreGdbServerSessions’
-    attribute should be set to ‘yes’.
- •	EnableSseContext: Flag if ‘yes’, then the ‘g’ context RSP packet will include floating point registers values. This parameter makes sense only for Intel family targets.
- •	heuristicScanSize: this configures the debugger engine fast heuristic algorithm to decrease the scanned memory probe by the specified size, if the attribute value is *not* specified (or “0”), then the
   debugger engine won’t use the fast heuristic and fall back to the legacy heuristic that scan the entire memory looking for the PE DOS signature.
- •	targetDescriptionFile: specifies if the GDB server sends a target description header file before sending each separate xml file. This field is blank then the GDB server client won’t request the xml
-    architecture system register (e.g. Trace32 GDBs server that does not support sending architecture registers in a separate xml file).
- •	GdbServerConnectionParameters: Specifies GdbServer session parameters. These parameters are used to control the RSP GdbServer session between the ExdiGdbSrv.dll component and GdbServer.
- •	MultiCoreGdbServerSessions: Flag If ‘yes’, then we will have multi-core GdbServer session (the one used by T32-GdbServer Back-End). If ‘no’, then we will communicate only with one instance of the GdbServer.
- •	MaximumGdbServerPacketLength: This is the maximum GdbServer supported length for one packet.
- •	MaximumConnectAttempts: This is the maximum connection attempts. It is used by the ExdiGdbSrv.dll when it tries to establish the RSP connection to the GdbServer.
- •	SendPacketTimeout: This is the RSP send timeout.
- •	ReceivePacketTimeout: This is the RSP receive timeout.
- •	HostNameAndPort: This is the connection string in the format `<hostname/ip address:Port number>`. There can be more than one GdbServer connection string (like T32 multi-core GdbServer session). The number of
 connection strings should match with the numbers of cores.
- •	ExdiGdbServerMemoryCommands: Specifies various ways of issuing the GDB memory commands, in order to obtain system registers values or read/write access memory at different exception CPU levels (e.g.
-  BMC-OpenOCD provides access to CP15 register via “`aarch64 mrs nsec/sec <access code>`” customized command).
- •	GdbSpecialMemoryCommand: if “yes”, then the GDB server supports customized memory commands (e.g. system register, this should be set for Trace32 GDB server).
- •	PhysicalMemory: if “yes”, then the GDB server supports customized commands for reading physical memory (it is set for Trace32 GDB server).
- •	SupervisorMemory: if “yes”, then the GDB server supports customized commands for reading supervisor memory (it is set for Trace32 GDB server).
- •	SpecialMemoryRegister: if “yes”, then the GDB server supports customized commands for reading system registers (it is set for Trace32 GDB server)
- •	SystemRegistersGdbMonitor: if “yes”, then the GDB server supports customized commands via GDB monitor command (it is set for BMC Open-OCD).
- •	SystemRegisterDecoding: if “yes”, then the GDB client accepts decoding the access code before sending the GDB monitor command.
- •	ExdiGdbServerRegisters: Specifies the specific architecture register core set.
- •	Architecture: CPU architecture of the defined registers set.
- •	FeatureNameSupported: This is the name of the system register group as it’s provided by the xml system register description file. It’s needed to identify the system register xml group that is part of the xml
-   file as it’s sent by the GDB server.
- •	SystemRegistersStart: This is to identify the first system register (low register number/order) that is reported as part of the core register set (e.g. on X64, QEMU does not report the x64 system register
-   set as a separated xml target description file, so system regs are part of the core registers).
- •	SystemRegistersEnd: This is to identify the last system register (high register number/order) that that is reported as part of the core register set.
- •	Name: Name of the register.
- •	Order: This is a number that identifies the index in the array of registers. This number will be used by the GDB client and server set/query (“`p<number>”/”q<number>`”) register packets.
- •	Size: This is the register size in bytes.

## Sample exdiConfigData.xml file

```xml
<ExdiTargets CurrentTarget = "QEMU">
<!-- QEMU SW simulator GDB server configuration -->
    <ExdiTargets CurrentTarget="QEMU">
    <!--  QEMU SW simulator GDB server configuration  -->
    <ExdiTarget Name="QEMU">
    <ExdiGdbServerConfigData agentNamePacket="" uuid="72d4aeda-9723-4972-b89a-679ac79810ef" displayCommPackets="yes" debuggerSessionByCore="no" enableThrowExceptionOnMemoryErrors="yes" qSupportedPacket="qSupported:xmlRegisters=aarch64,i386">
    <ExdiGdbServerTargetData targetArchitecture="ARM64" targetFamily="ProcessorFamilyARM64" numberOfCores="1" EnableSseContext="no" heuristicScanSize="0xfffe" targetDescriptionFile="target.xml"/>
    <GdbServerConnectionParameters MultiCoreGdbServerSessions="no" MaximumGdbServerPacketLength="1024" MaximumConnectAttempts="3" SendPacketTimeout="100" ReceivePacketTimeout="3000">
    <Value HostNameAndPort="LocalHost:1234"/>
    </GdbServerConnectionParameters>
    <ExdiGdbServerMemoryCommands GdbSpecialMemoryCommand="no" PhysicalMemory="no" SupervisorMemory="no" HypervisorMemory="no" SpecialMemoryRegister="no" SystemRegistersGdbMonitor="no" SystemRegisterDecoding="no"> </ExdiGdbServerMemoryCommands>
<ExdiGdbServerRegisters Architecture = "ARM64" FeatureNameSupported = "sys">
    <Entry Name ="X0"  Order = "0" Size = "8" />
    <Entry Name ="X1"  Order = "1" Size = "8" />
    <Entry Name ="X2"  Order = "2" Size = "8" />
    <Entry Name ="X3"  Order = "3" Size = "8" />
    <Entry Name ="X4"  Order = "4" Size = "8" />
    <Entry Name ="X5"  Order = "5" Size = "8" />
    <Entry Name ="X6"  Order = "6" Size = "8" />
    <Entry Name ="X7"  Order = "7" Size = "8" />
    <Entry Name ="X8"  Order = "8" Size = "8" />
    <Entry Name ="X9"  Order = "9" Size = "8" />
    <Entry Name ="X10" Order = "a"  Size = "8" />
    <Entry Name ="X11" Order = "b"  Size = "8" />
    <Entry Name ="X12" Order = "c"  Size = "8" />
    <Entry Name ="X13" Order = "d"  Size = "8" />
    <Entry Name ="X14" Order = "e"  Size = "8" />
    <Entry Name ="X15" Order = "f"  Size = "8" />
    <Entry Name ="X16" Order = "10" Size = "8" />
    <Entry Name ="X17" Order = "11" Size = "8" />
    <Entry Name ="X18" Order = "12" Size = "8" />
    <Entry Name ="X19" Order = "13" Size = "8" />
    <Entry Name ="X20" Order = "14" Size = "8" />
    <Entry Name ="X21" Order = "15" Size = "8" />
    <Entry Name ="X22" Order = "16" Size = "8" />
    <Entry Name ="X23" Order = "17" Size = "8" />
    <Entry Name ="X24" Order = "18" Size = "8" />
    <Entry Name ="X25" Order = "19" Size = "8" />
    <Entry Name ="X26" Order = "1a" Size = "8" />
    <Entry Name ="X27" Order = "1b" Size = "8" />
    <Entry Name ="X28" Order = "1c" Size = "8" />
    <Entry Name ="fp"  Order = "1d" Size = "8" />
    <Entry Name ="lr"  Order = "1e" Size = "8" />
    <Entry Name ="sp"  Order = "1f" Size = "8" />
    <Entry Name ="pc"  Order = "20" Size = "8" />
    <Entry Name ="cpsr" Order = "21" Size = "8" />
    <Entry Name ="V0" Order = "22" Size = "16" />
    <Entry Name ="V1" Order = "23" Size = "16" />
    <Entry Name ="V2" Order = "24" Size = "16" />
    <Entry Name ="V3" Order = "25" Size = "16" />
    <Entry Name ="V4" Order = "26" Size = "16" />
    <Entry Name ="V5" Order = "27" Size = "16" />
    <Entry Name ="V6" Order = "28" Size = "16" />
    <Entry Name ="V7" Order = "29" Size = "16" />
    <Entry Name ="V8" Order = "2a" Size = "16" />
    <Entry Name ="V9" Order = "2b" Size = "16" />
    <Entry Name ="V10" Order = "2c" Size = "16" />
    <Entry Name ="V11" Order = "2d" Size = "16" />
    <Entry Name ="V12" Order = "2e" Size = "16" />
    <Entry Name ="V13" Order = "2f" Size = "16" />
    <Entry Name ="V14" Order = "30" Size = "16" />
    <Entry Name ="V15" Order = "31" Size = "16" />
    <Entry Name ="V16" Order = "32" Size = "16" />
    <Entry Name ="V17" Order = "33" Size = "16" />
    <Entry Name ="V18" Order = "34" Size = "16" />
    <Entry Name ="V19" Order = "35" Size = "16" />
    <Entry Name ="V20" Order = "36" Size = "16" />
    <Entry Name ="V21" Order = "37" Size = "16" />
    <Entry Name ="V22" Order = "38" Size = "16" />
    <Entry Name ="V23" Order = "39" Size = "16" />
    <Entry Name ="V24" Order = "3a" Size = "16" />
    <Entry Name ="V25" Order = "3b" Size = "16" />
    <Entry Name ="V26" Order = "3c" Size = "16" />
    <Entry Name ="V27" Order = "3d" Size = "16" />
    <Entry Name ="V28" Order = "3e" Size = "16" />
    <Entry Name ="V29" Order = "3f" Size = "16" />
    <Entry Name ="V30" Order = "3f" Size = "16" />
    <Entry Name ="V31" Order = "3f" Size = "16" />
    <Entry Name ="fpsr" Order = "40" Size = "4" />
    <Entry Name ="fpcr" Order = "41" Size = "4" />
</ExdiGdbServerRegisters>


<!-- x64 GDB server core resgisters -->
<ExdiGdbServerRegisters Architecture = "X64" FeatureNameSupported = "sys" SystemRegistersStart = "18" SystemRegistersEnd = "20" >
    <Entry Name ="rax" Order = "0" Size ="8" />
    <Entry Name ="rbx" Order = "1" Size ="8" />
    <Entry Name ="rcx" Order = "2" Size ="8" />
    <Entry Name ="rdx" Order = "3" Size ="8" />
    <Entry Name ="rsi" Order = "4" Size ="8" />
    <Entry Name ="rdi" Order = "5" Size ="8" />
    <Entry Name ="rbp" Order = "6" Size ="8" />
    <Entry Name ="rsp" Order = "7" Size ="8" />
    <Entry Name ="r8"  Order = "8" Size ="8" />
    <Entry Name ="r9"  Order = "9" Size ="8" />
    <Entry Name ="r10" Order = "a" Size ="8" />
    <Entry Name ="r11" Order = "b" Size ="8" />
    <Entry Name ="r12" Order = "c" Size ="8" />
    <Entry Name ="r13" Order = "d" Size ="8" />
    <Entry Name ="r14" Order = "e" Size ="8" />
    <Entry Name ="r15" Order = "f" Size ="8" />
    <Entry Name ="rip" Order = "10" Size ="8" />
    <!-- <flags id="x64_eflags" size="4">
<field name="" start="22" end="31"/>
<field name="ID" start="21" end="21"/>
<field name="VIP" start="20" end="20"/>
<field name="VIF" start="19" end="19"/>
<field name="AC" start="18" end="18"/>
<field name="VM" start="17" end="17"/>
<field name="RF" start="16" end="16"/>
<field name="" start="15" end="15"/>
<field name="NT" start="14" end="14"/>
<field name="IOPL" start="12" end="13"/>
<field name="OF" start="11" end="11"/>
<field name="DF" start="10" end="10"/>
<field name="IF" start="9" end="9"/>
<field name="TF" start="8" end="8"/>
<field name="SF" start="7" end="7"/>
<field name="ZF" start="6" end="6"/>
<field name="" start="5" end="5"/>
<field name="AF" start="4" end="4"/>
<field name="" start="3" end="3"/>
<field name="PF" start="2" end="2"/>
<field name="" start="1" end="1"/>
<field name="CF" start="0" end="0"/>
    </flags> -->
    <Entry Name ="eflags" Order = "11" Size ="4" />

    <!-- Segment registers -->
    <Entry Name ="cs" Order = "12" Size ="4" />
    <Entry Name ="ss" Order = "13" Size ="4" />
    <Entry Name ="ds" Order = "14" Size ="4" />
    <Entry Name ="es" Order = "15" Size ="4" />
    <Entry Name ="fs" Order = "16" Size ="4" />
    <Entry Name ="gs" Order = "17" Size ="4" />

    <!-- Segment descriptor caches and TLS base MSRs -->
    <!--Entry Name ="cs_base" Order = "18" Size="8"/
    <Entry Name ="ss_base" Order = "18" Size ="8" />
    <Entry Name ="ds_base" Order = "19" Size ="8" />
    <Entry Name ="es_base" Order = "1a" Size ="8" /> -->
    <Entry Name ="fs_base" Order = "18" Size ="8" />
    <Entry Name ="gs_base" Order = "19" Size ="8" />
    <Entry Name ="k_gs_base" Order = "1a" Size ="8" />

    <!-- Control registers -->
    <!-- the cr0 register format fields:
    <flags id="x64_cr0" size="8">
    <field name="PG" start="31" end="31"/>
    <field name="CD" start="30" end="30"/>
    <field name="NW" start="29" end="29"/>
    <field name="AM" start="18" end="18"/>
    <field name="WP" start="16" end="16"/>
    <field name="NE" start="5" end="5"/>
    <field name="ET" start="4" end="4"/>
    <field name="TS" start="3" end="3"/>
    <field name="EM" start="2" end="2"/>
    <field name="MP" start="1" end="1"/>
    <field name="PE" start="0" end="0"/>
    </flags> -->
    <Entry Name ="cr0" Order = "1b" Size ="8" />
    <Entry Name ="cr2" Order = "1c" Size ="8" />

    <!-- the cr3 register format fields:
    <flags id="x64_cr3" size="8">
<field name="PDBR" start="12" end="63"/>
<field name="PCID" start="0" end="11"/>
    </flags> -->
    <Entry Name ="cr3" Order = "1d" Size ="8" />

    <!-- the cr4 register format fields:
    <flags id="x64_cr4" size="8">
<field name="PKE" start="22" end="22"/>
<field name="SMAP" start="21" end="21"/>
<field name="SMEP" start="20" end="20"/>
<field name="OSXSAVE" start="18" end="18"/>
<field name="PCIDE" start="17" end="17"/>
<field name="FSGSBASE" start="16" end="16"/>
<field name="SMXE" start="14" end="14"/>
<field name="VMXE" start="13" end="13"/>
<field name="LA57" start="12" end="12"/>
<field name="UMIP" start="11" end="11"/>
<field name="OSXMMEXCPT" start="10" end="10"/>
<field name="OSFXSR" start="9" end="9"/>
<field name="PCE" start="8" end="8"/>
<field name="PGE" start="7" end="7"/>
<field name="MCE" start="6" end="6"/>
<field name="PAE" start="5" end="5"/>
<field name="PSE" start="4" end="4"/>
<field name="DE" start="3" end="3"/>
<field name="TSD" start="2" end="2"/>
<field name="PVI" start="1" end="1"/>
<field name="VME" start="0" end="0"/>
    </flags> -->
    <Entry Name ="cr4" Order = "1e" Size ="8" />
    <Entry Name ="cr8" Order = "1f" Size ="8" />

    <!-- the efer register format fields:
    <flags id="x64_efer" size="8">
    <field name="TCE" start="15" end="15"/>
    <field name="FFXSR" start="14" end="14"/>
    <field name="LMSLE" start="13" end="13"/>
    <field name="SVME" start="12" end="12"/>
    <field name="NXE" start="11" end="11"/>
    <field name="LMA" start="10" end="10"/>
    <field name="LME" start="8" end="8"/>
    <field name="SCE" start="0" end="0"/>
    </flags> -->
    <Entry Name ="efer" Order = "20" Size ="8"/>

    <!-- x87 FPU -->
    <Entry Name ="st0" Order = "21" Size ="10" />
    <Entry Name ="st1" Order = "22" Size ="10" />
    <Entry Name ="st2" Order = "23" Size ="10" />
    <Entry Name ="st3" Order = "24" Size ="10" />
    <Entry Name ="st4" Order = "25" Size ="10" />
    <Entry Name ="st5" Order = "26" Size ="10" />
    <Entry Name ="st6" Order = "27" Size ="10" />
    <Entry Name ="st7" Order = "28" Size ="10" />
    <Entry Name ="fctrl" Order = "29" Size ="4" />
    <Entry Name ="fstat" Order = "2a" Size ="4" />
    <Entry Name ="ftag"  Order = "2b" Size ="4" />
    <Entry Name ="fiseg" Order = "2c" Size ="4" />
    <Entry Name ="fioff" Order = "2d" Size ="4" />
    <Entry Name ="foseg" Order = "2e" Size ="4" />
    <Entry Name ="fooff" Order = "2f" Size ="4" />
    <Entry Name ="fop" Order = "30" Size ="4" />
    <Entry Name ="xmm0" Order = "31" Size ="16"  />
    <Entry Name ="xmm1" Order = "32" Size ="16"  />
    <Entry Name ="xmm2" Order = "33" Size ="16"  />
    <Entry Name ="xmm3" Order = "34" Size ="16"  />
    <Entry Name ="xmm4" Order = "35" Size ="16"  />
    <Entry Name ="xmm5" Order = "36" Size ="16"  />
    <Entry Name ="xmm6" Order = "37" Size ="16"  />
    <Entry Name ="xmm7" Order = "38" Size ="16"  />
    <Entry Name ="xmm8" Order = "39" Size ="16"  />
    <Entry Name ="xmm9" Order = "3a" Size ="16"  />
    <Entry Name ="xmm10" Order = "3b" Size ="16"  />
    <Entry Name ="xmm11" Order = "3c" Size ="16"  />
    <Entry Name ="xmm12" Order = "3d" Size ="16"  />
    <Entry Name ="xmm13" Order = "3e" Size ="16"  />
    <Entry Name ="xmm14" Order = "3f" Size ="16"  />
    <Entry Name ="xmm15" Order = "40" Size ="16"  />
    
    <!-- the mxcsr register format fields:
    <flags id="x64_mxcsr" size="4">
<field name="IE" start="0" end="0"/>
<field name="DE" start="1" end="1"/>
<field name="ZE" start="2" end="2"/>
<field name="OE" start="3" end="3"/>
<field name="UE" start="4" end="4"/>
<field name="PE" start="5" end="5"/>
<field name="DAZ" start="6" end="6"/>
<field name="IM" start="7" end="7"/>
<field name="DM" start="8" end="8"/>
<field name="ZM" start="9" end="9"/>
<field name="OM" start="10" end="10"/>
<field name="UM" start="11" end="11"/>
<field name="PM" start="12" end="12"/>
<field name="FZ" start="15" end="15"/>
    </flags> -->
    <Entry Name ="mxcsr" Order = "41" Size ="4" />

</ExdiGdbServerRegisters>
    </ExdiGdbServerConfigData>
    </ExdiTarget>
    </ExdiTargets>
</ExdiTargets>
```


## Sample of PowerShell script to inject paravirtualized drivers and enable KDNET into a Windows VHDX image

```ps1
    param (
[string]$VhdxPath = "D:\image.vhdx"
    )

    function setup_kdnet {
$efipart = $disk | Get-Partition | Where-Object GptType -Eq "{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}"

$efipart | Set-Partition -NewDriveLetter 'S'

$cmdexp = "bcdedit /store s:\EFI\Microsoft\Boot\BCD /dbgsettings net hostip:" + $kdhostipaddr + " port:50004 key:1.2.3.4"
Invoke-Expression -Command:$cmdexp

$cmdexp = 'bcdedit /store s:\EFI\Microsoft\Boot\BCD /set "{bootmgr}" testsigning on'
Invoke-Expression -Command:$cmdexp

$cmdexp = 'bcdedit /store s:\EFI\Microsoft\Boot\BCD /set "{default}" testsigning on'
Invoke-Expression -Command:$cmdexp

$cmdexp = 'bcdedit /store s:\EFI\Microsoft\Boot\BCD /set "{bootmgr}" bootdebug off'
Invoke-Expression -Command:$cmdexp

$cmdexp = 'bcdedit /store s:\EFI\Microsoft\Boot\BCD /set "{default}" bootdebug off'
Invoke-Expression -Command:$cmdexp

$cmdexp = 'bcdedit /store s:\EFI\Microsoft\Boot\BCD /set "{default}" debug off'
Invoke-Expression -Command:$cmdexp

$efipart | Remove-PartitionAccessPath -AccessPath "S:\"
    }

    function install_qemu_drivers {
$ospart = $disk | Get-Partition | Where-Object GptType -Eq "{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}"
$ospart | Set-Partition -NewDriveLetter 'O'

try {
    $cmdexp = 'dism /add-driver /image:O:\ /driver:..\drivers\viostor\ARM64\viostor.inf'
    Invoke-Expression -Command:$cmdexp

    $cmdexp = 'dism /add-driver /image:O:\ /driver:..\drivers\netkvm\ARM64\netkvm.inf'
    Invoke-Expression -Command:$cmdexp
}
finally {
    $ospart | Remove-PartitionAccessPath -AccessPath "O:\"
}
    }

    function add_debugger {
$ospart = $disk | Get-Partition | Where-Object GptType -Eq "{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}"
$ospart | Set-Partition -NewDriveLetter 'O'

try {
    copy -Recurse \\dbg\privates\latest\uncompressed\arm64\DbgSrv O:\DbgSrv
}
finally {
    $ospart | Remove-PartitionAccessPath -AccessPath "O:\"
}
    }

    function add_unattend {
$ospart = $disk | Get-Partition | Where-Object GptType -Eq "{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}"
$ospart | Set-Partition -NewDriveLetter 'O'

try {
    copy unattend.xml O:\
}
finally {
    $ospart | Remove-PartitionAccessPath -AccessPath "O:\"
}
    }

    $hypervnet = Get-NetIPAddress | Where-Object {($_.InterfaceAlias -like "Ethernet*" -and $_.AddressFamily -Eq "IPv4")}
    $kdhostipaddr = $hypervnet.IPv4Address

    $diskimage = Mount-DiskImage -NoDriveLetter -PassThru $VhdxPath -ErrorAction Stop
    $disk = $diskimage | Get-Disk

    setup_kdnet
    install_qemu_drivers
    add_debugger
    add_unattend
    Dismount-DiskImage -InputObject $diskimage
```

## Sample of QEMU x64 Windows VM launching script

```bat
    REM
    REM  This script is used to run a Windows x64 VM on QEMU that is hosted by a Windows x64 host system
    REM  The Host system is a HP z420 developer box with Intel(R) Xeon(R) CPU.
    REM
    set EXECUTABLE=qemu-system-x86_64
    set MACHINE=-m 6G -smp 4

    REM No acceleration
    REM generic cpu emulation.
    REM to find out which CPU types are supported by the QEMU version on your system, then run:
    REM	 qemu-system-x86_64.exe -cpu help
    REM the see if your host system CPU is listed
    REM

    set CPU=-machine q35 

    REM Enables x64 UEFI-BIOS that will be used by QEMU :
    set BIOS=-bios D:\temp\firmware\OVMF.fd

    REM  Use regular GFX simulation
    set GFX=-device ramfb -device VGA 
    set USB_CTRL=-device usb-ehci,id=usbctrl
    set KEYB_MOUSE=-device usb-kbd -device usb-tablet

    REM # The following line enable the full-speed HD controller (requires separate driver)
    REM # Following line uses the AHCI controller for the Virtual Hard Disk:
    set DRIVE0=-device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0

    REM
    REM This will set the Windows VM x64 disk image that will be launched by QEMU
    REM The disk image is in the qcow2 format accepted by QEMU.
    REM You get the .qcow2 image, once you get the VHDX Windows VM x64 image (i.e. form \\winbuilds\release\..|VHDX\
    REM and apply the script to inject the virtio x64 drivers and then run the 
    REM the QEMU tool to convert the .VHDX image to .qcow2 format
    REM 	i.e. 
    REM	qemu-img convert -c -p -O qcow2 Windows_VM_VHDX_with_injected_drivers_file.vhdx file.qcow2
    REM file : points to the specified qcow2 image path (D:\temp\vhdx_x64_Cobalt\x64_image_qcow2_for_windows\basex64Client.qcow2).
    REM
    set DISK0=-drive id=disk,file=D:\temp\x64_image_qcow2_for_windows\basex64Client.qcow2,if=none

    REM
    REM for kdnet on, then best option:
    REM   NETWORK0="-netdev user,id=net0,hostfwd=tcp::53389-:3389,hostfwd=tcp::50001-:50001 -device virtio-net,netdev=net0,disable-legacy=on"
    REM
    set NETHOST=-netdev user,id=net0,hostfwd=tcp::3589-:3389
    set NETGUEST=-device e1000,netdev=net0

    REM # The following line should enable the Daemon (instead of interactive)
    set DEAMON=-daemonize"
    %EXECUTABLE% %MACHINE% %CPU% %BIOS% %GFX% %USB_CTRL% %DRIVE0% %DISK0% %NETHOST% %NETGUEST%
```

## Sample of QEMU ARM64 Linux VM launching script

```sh
#!/bin/bash
EXECUTABLE=qemu-system-aarch64
MACHINE="-m 6G -smp 8"

#
# CPU and UEFI BIOS.
#
CPU="-cpu cortex-a57 -machine virt,virtualization=true"

# Enables UEFI BIOS:
BIOS="-bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd"


#
# Display, keyboard and mouse.
#
# Note that :6 means port 5906    
GFX="-device ramfb -device VGA"
USB_CTRL="-device usb-ehci,id=usbctrl"
KEYB_MOUSE="-device usb-kbd -device usb-tablet"

#
# Hard disk controller and image.
#
# The following line uses the accelerated VirtIO Storage controller (the only one supported in ARM64)
# This requires drivers, that MUST be installed in the VHDX via :
# dism /add-driver /image:O:\ /driver:drivers\viostor\ARM64\viostor.inf
DRIVE0="-device virtio-blk-pci,drive=hd0"
DISK0="-drive if=none,file=/Virtual_Machines/BASE_AARCH64/file.qcow2,id=hd0,aio=native,cache.direct=on"

#
# Network
#
# Install VirtIo drivers via
# dism /add-driver /image:O:\ /driver:drivers\netkvm\ARM64\netkvm.inf
#

# Redirect RDP port 3389 in host port 63389
NETHOST="-netdev user,id=net0,hostfwd=tcp::63389-:3389"
NETGUEST="-device virtio-net,netdev=net0,disable-legacy=on"

# The following line should enable the Daemon (instead of interactive)
# DEAMON="-daemonize"

$EXECUTABLE \
    $MACHINE \
    $CPU \
    $BIOS \
    $MONITOR \
    $SERIAL \
    $GFX \
    $USB_CTRL \
    $DRIVE0 $DISK0 \
    $DRIVE1 $DISK1 \
    $DRIVE2 $DISK2 \
    $CDROM \
    $KEYB_MOUSE \
    $NETHOST $NETGUEST \
    $DEAMON

```
