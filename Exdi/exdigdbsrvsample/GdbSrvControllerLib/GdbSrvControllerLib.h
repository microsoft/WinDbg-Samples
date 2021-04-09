//----------------------------------------------------------------------------
//
// GdbSrvControllerLib.h
//
// A class that runs a GdbServer client that services the DbgEng-Exdi debugger 
// requests.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once
#include <string>
#include <map>
#include <vector>
#include <regex>
#include "BufferWrapper.h"
#include "HandleHelpers.h"
#include "GdbSrvRspClient.h"

namespace GdbSrvControllerLib
{
    //  Maximum number of supported GdbServer CPU registers by supported architecture.
    const int MAX_REG_X86_NUMBER = 40;
    const int MAX_REG_AMD64_NUMBER = 55;
    const int MAX_REG_ARM32_NUMBER = 50;
    const int MAX_REG_ARM64_NUMBER = 68;

    typedef enum 
    {
        UNKNOWN_ARCH, 
        X86_ARCH, 
        AMD64_ARCH, 
        ARM32_ARCH, 
        ARM64_ARCH
    } TargetArchitecture;

    //  This type indicates the signal types returned by the DbgServer
    typedef enum
    {
        TARGET_UNKNOWN = 0,
        //  The target process has been terminated.
        TARGET_BREAK_SIGUP = 1,
        //  The debugger sends a CTRL_break
        TARGET_BREAK_SIGINT = 2,
        //  The target is broken because the debugger requests a break.
        TARGET_BREAK_SIGTRAP = 5,
        //  The process exited
        TARGET_PROCESS_EXIT = 6,
        //  END marker
        TARGET_MARKER
    } TARGET_HALTED;

    //  This structure indicates the register description for each architecture
    //  This will be used for processing the DbgServer register reply commands
    //  The order matches with the order how it is sent by the GdbServer stub.
    struct RegistersStruct
    {
        //  Register Name description
        std::string name;
        //  The register index (ascii hex decimal number)
        std::string nameOrder;
        //  Register size in bytes
        size_t registerSize;
    };
    typedef std::map<std::string, std::string> gdbRegisterMapOrder;

    typedef ULONGLONG AddressType;

    //  This structure indicates the fields of the stop reply reason response.
    typedef struct
    {
        TARGET_HALTED stopReason;
        ULONG         processorNumber;
        AddressType   currentAddress;
        //  Stop reply packet response status
        struct {                   
            WORD isSAAPacket: 1;    //  Set if the stop reply packet is S AA format
            WORD isTAAPacket: 1;    //  Set if the stop reply packet is T AA format
            WORD isWAAPacket: 1;    //  Set if the stop reply packet is W AA format
            WORD isThreadFound: 1;  //  Set if the stop reply packet contains the 'thread' number (core processor number)
            WORD isPcRegFound: 1;   //  Set if the PC register was found in the response
            WORD isPowerDown: 1;    //  Set if the stop reply packet is 'S00' (Power down or target running).
            WORD isCoreRunning: 1;  //  Set if the stop reply packet is 'OK' (the Core is running or it's not unknown state)
            WORD fUnUsed: 9;
        } status;

    } StopReplyPacketStruct;
         
    //
    //  This is used to set the type of memory packet that will be be sent to the Gdb Server.
    //  In general, these flags are mutually exclusive, but it's up to the implementation to use them.
    //
    typedef struct 
    {
        WORD isPhysical: 1;        //  Set if the query is to access physical memory
        WORD isSupervisor: 1;      //  Set if the query is to access supervisor/system mode memory 
        WORD isSpecialRegs: 1;     //  Set if the query is to access memory pointed by Special registers
        WORD isData:1;             //  Set if the query is to access user mode memory.
        WORD isHypervisor:1;       //  Set if the query is to access hypervisor memory.
        WORD fUnUsed: 11;
    } memoryAccessType;

    //
    //  This class implements the High level functionality supported by the GdbServer stub.
    //  Basically, it translates the DbgEng-Exdi requested functionality to GdbServer commands.
    //
    class GdbSrvController
    {
    public:
        virtual ~GdbSrvController();

        //  Connect to the GdbServer.
        bool ConnectGdbSrv();
                                                          
        //  Check if the GdbServer is still connected.
        bool CheckGdbSrvAlive(_Out_ HRESULT & error);

        //  Configure the GdbServer communication session.
        bool ConfigureGdbSrvCommSession(_In_ bool fDisplayCommData, _In_ int core);

        //  Execute a basic GdbServer command.
        virtual std::string ExecuteCommand(_In_ LPCSTR pCommand);

        //  Execute a basic GdbServer command with extended command attributes.
        virtual std::string ExecuteCommandEx(_In_ LPCSTR pCommand, _In_ bool isExecCmd, _In_ size_t stringSize);
        
        //  Execute a basic GdbServer command with extended command attributes on a particular processor core.
        virtual std::string ExecuteCommandOnProcessor(_In_ LPCSTR pCommand, _In_ bool isExecCmd, _In_ size_t stringSize, 
                                                      _In_ unsigned processor);

        //  Handle the responses for the asynchronous commnads (the stop reason reply responses).
        bool HandleAsynchronousCommandResponse(_In_ const std::string & cmdResponse,
                                               _Out_ StopReplyPacketStruct * pRspPacket);

        //  Interrupt the target by posting the interrupt character sequence.
        bool InterruptTarget();

        //  Determines if the target is halted due to a debug(signal) break.
        bool IsTargetHalted();
        
        //  Retrieves the last used processor number.
        unsigned GetLastKnownActiveCpu();

        //  Set the last Active Cpu
        void SetLastKnownActiveCpu(_In_ unsigned cpu); 

        //  Obtains the location offset of the Windows OS specific stored KPCR array.
        AddressType GetKpcrOffset(_In_ unsigned processorNumber);

        //  Get the number of processors supported by the target.
		unsigned GetProcessorCount();
		
        //  Get the general GdbServer-RSP protocol response type. 
        RSP_Response_Packet GetRspResponse(_In_ const std::string & reply);
              
        //  Obtains the stored target architecture.
        TargetArchitecture GetTargetArchitecture();

        //  Request the current value for all target CPU registers.
        std::map<std::string, std::string> QueryAllRegisters(_In_ unsigned processorNumber);

        
        //  Request a sub-set of all target CPU registers.
        std::map<std::string, std::string> QueryRegisters(_In_ unsigned processorNumber,
                                                          _In_reads_(numberOfElements) const char * registerNames[],
                                                          _In_ const size_t numberOfElements);

        //  Utility function to get a 64 bit register value.
        static ULONGLONG ParseRegisterValue(_In_ const std::string &stringValue);
        
        //  Utility function to get a 32 bit register value.
        static DWORD ParseRegisterValue32(_In_ const std::string &stringValue);

        //  Utility function to get a variable size regiter value (the register can be a vector).
        static void ParseRegisterVariableSize(_In_ const std::string &registerValue, 
                                              _Out_writes_bytes_(registerAreaLength) BYTE pRegisterArea[],
                                              _In_ int registerAreaLength);

        //  Read the target virtual memory.
        SimpleCharBuffer ReadMemory(_In_ AddressType address, _In_ size_t size, _In_ const memoryAccessType memType);

        //  Request the GDbServer supported features. 
        bool ReqGdbServerSupportedFeatures();

        //  Request the Windows OS Thread information block.
        bool RequestTIB();

        //  Reports the reason of the break.
        TARGET_HALTED ReportReasonTargetHalted(_Out_ StopReplyPacketStruct * pStopReply);

        //  Request restart the target.
        bool RestartGdbSrvTarget();

        //  Store the KPCR offset for later usage.
        void SetKpcrOffset(_In_ unsigned processorNumber, _In_ AddressType kpcrOffset);

        //  Set the value of the specific register set.
        void SetRegisters(_In_ unsigned processorNumber, _In_ const std::map<std::string, AddressType> &registerValues, 
                          _In_ bool isRegisterValuePtr);

        //  Stores the target architecture.
        void SetTargetArchitecture(_In_ TargetArchitecture targetArch);

        //  Stores the pointer of the text logging class.
        void SetTextHandler(_In_ IGdbSrvTextHandler * pHandler);

        //  Set the thread/processor number
        bool SetThreadCommand(_In_ unsigned processorNumber, _In_ const char * pOperation);
                
        //  Close the connection with the GdbServer.
        void ShutdownGdbSrv();

        //  Write a value to the target virtual memory at specific address.
        bool WriteMemory(_In_ AddressType address, _In_ size_t size, _In_ const void * pRawBuffer, 
                         _Out_ DWORD * pdwBytesWritten, _In_ const memoryAccessType memType);

        //  Get the number of RSP GdbServer connections.
        unsigned GetNumberOfRspConnections();

        //  Execute the same command on multiple processors. Accept only the first response.
        std::string ExecuteCommandOnMultiProcessors(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize);

        //  Display a message on the command line log windows
        void DisplayLogEntry(_In_reads_bytes_(readSize) const char * pBuffer, _In_ size_t readSize);

        //  Execute a GdbSrv function.
        bool ExecuteExdiFunction(_In_ unsigned core, _In_ LPCWSTR pFunctionToExecute);

        //  Execute a GdbSrv monitor command.
        SimpleCharBuffer ExecuteExdiGdbSrvMonitor(_In_ unsigned dwProcessorNumber, _In_ LPCWSTR pFunctionToExecute);

        //  Create and fill the Neon register array
        void CreateNeonRegisterNameArray(_In_ const std::string & registerName,
                                         _Out_writes_bytes_(numberOfRegArrayElem) std::unique_ptr<char> pRegNameArray[],
                                         _In_ size_t numberOfRegArrayElem);

        //  Get the first thread/core index obtained from the Gdb Server qfThreadInfo packet response
        int GetFirstThreadIndex();

        //  GetPacketType based on the current memory type
        void GetMemoryPacketType(_In_ DWORD64 cpsrRegValue, _Out_ memoryAccessType * pMemType);

        //  Check if the current architecture is 64 bits
        bool Is64BitArchitecture();

        // Read the MSR registers
        HRESULT ReadMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _Out_ ULONG64 * pValue);

        // Write to the MSR register
        HRESULT WriteMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _In_ ULONG64 value);

    protected:
        bool IsReplyOK(_In_ const std::string & reply);

        bool IsReplyError(_In_ const std::string & reply);

        bool IsStopReply(_In_ const std::string & reply);

        GdbSrvController(_In_ const std::vector<std::wstring> &coreConnectionParameters);

    private:
        class GdbSrvControllerImpl;
        std::unique_ptr <GdbSrvControllerImpl> m_pGdbSrvControllerImpl;
    };
}