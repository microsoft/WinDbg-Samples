//----------------------------------------------------------------------------
//
// KDController.h
//
// A class allowing running KD.EXE and sending commands to it.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "KDController.h"
#include "ExceptionHelpers.h"
#include "HandleHelpers.h"
#include <new>
#include <algorithm>
#include <string>

using namespace KDControllerLib;

KDController::KDController(_In_ HANDLE processHandle, _In_ HANDLE stdInput, _In_ HANDLE stdOutput)
    : m_processHandle(processHandle)
    , m_stdInput(stdInput)
    , m_stdOutput(stdOutput)
    , m_stdoutReader(stdOutput)
    , m_pTextHandler(nullptr)
    , m_kdPromptRegex("\n(|[0-9]+: )kd> ")  //The prompt can be either "kd> " or "#: kd>" where # is the core number.
	, m_cachedProcessorCount(0)
    , m_lastKnownActiveCpu(0)
{
    //If we run inside the WinDbg process and WinDbg gets closed without ending the session cleanly, we still want
    //to terminate the underlying kd.exe in order to make the machine connection available for subsequent debug
    //sessions. Assigning the process to a job with a JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag ensures exactly that:
    //when the job handle is closed (either explicitly or implicitly when our process exits), the kd process will
    //be terminated.
    HANDLE job = CreateJobObject(NULL, NULL);
    if (job != nullptr)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInformation = { 0 };
        jobInformation.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jobInformation, sizeof(jobInformation)))
        {
            if (AssignProcessToJobObject(job, m_processHandle.Get()))
            {
                m_jobHandle.Attach(job);
            }
        }

        if (!m_jobHandle.IsValid())
        {
            CloseHandle(job);
            job = nullptr;
        }
    }
}

void KDController::ShutdownKD()
{
    if (m_stdInput.IsValid() && m_stdOutput.IsValid())
    {
        char ctrlB = 2;
        DWORD done;
        WriteFile(m_stdInput.Get(), &ctrlB, 1, &done, nullptr);
        assert(done == 1);

        m_stdInput.Close();
        m_stdOutput.Close();

        if (WaitForSingleObject(m_processHandle.Get(), 100) != WAIT_OBJECT_0)
        {
            //If KD.EXE did not exit after receiving Ctrl-B due to some reason we terminate it forcibly
            //so that it releases the pipe handle and we can start another instance.
            BOOL terminated = TerminateProcess(m_processHandle.Get(), static_cast<UINT>(-1));
            assert(terminated == TRUE);
            UNREFERENCED_PARAMETER(terminated);
        }
    }
}

KDController::~KDController()
{
    ShutdownKD();

    delete m_pTextHandler;
    m_pTextHandler = nullptr;
}

void KDController::SetTextHandler(_In_ IKDTextHandler *pHandler)
{
    assert(pHandler != m_pTextHandler && pHandler != nullptr);
    delete m_pTextHandler;
    m_pTextHandler = pHandler;
}

std::string KDController::ExecuteCommand(_In_ LPCSTR pCommand)
{
    if (pCommand == nullptr)
    {
        throw _com_error(E_POINTER);
    }

    if (m_pTextHandler != nullptr)
    {
        m_pTextHandler->HandleText(KDTextType::Command, pCommand);
    }

    DWORD bytesWritten;

    if (!WriteFile(m_stdInput.Get(), pCommand, static_cast<DWORD>(strlen(pCommand)), &bytesWritten, nullptr))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (!WriteFile(m_stdInput.Get(), "\n", 1, &bytesWritten, nullptr))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }

    std::string result = ReadStdoutUntilDelimiter();
    
    if (m_pTextHandler != nullptr)
    {
        m_pTextHandler->HandleText(KDTextType::CommandOutput, result.c_str());
    }

    return result;
}

std::string KDController::ReadStdoutUntilDelimiter()
{
    BufferedStreamReader::MatchCollection matches;
    std::string result = m_stdoutReader.Read(m_kdPromptRegex, &matches);
    if (matches.size() >= 1)
    {
        m_lastKnownActiveCpu = atoi(matches[0].c_str());
    }
    return result;
}

void KDController::WaitForInitialPrompt()
{
    ReadStdoutUntilDelimiter();
}

std::map<std::string, std::string> KDController::QueryAllRegisters(_In_ unsigned processorNumber)
{
    //We expect a multi-line reply looking like 'rax=00...0 rbx=00...1'.
    //As we want to keep this parser as simple as possible, we will simply search for all instances of the '='
    //sign and treat the word to the left as the register name and the word to the right as the register value.

	char command[32] = "r";
	if (processorNumber != -1)
	{
		_snprintf_s(command, _TRUNCATE, "%dr", processorNumber);
	}

    std::string reply = ExecuteCommand(command);
    std::map<std::string, std::string> result;

    //Once we replace newlines with spaces we don't need to care how many registers are displayed per line of output.
    std::replace(reply.begin(), reply.end(), '\n', ' ');

    //Iterate over all occurances of the '=' sign in the string
    for(size_t i = reply.find('='); i != std::string::npos; i = reply.find('=', i + 1))
    {
        size_t registerNameStart = reply.rfind(' ', i);
        size_t registerValueEnd = reply.find(' ', i);

        if (registerNameStart == std::string::npos)
        {
            registerNameStart = 0;  //The very first register.
        }
        else
        {
            ++registerNameStart;
        }

        result[reply.substr(registerNameStart, i - registerNameStart)] = 
                reply.substr(i + 1, registerValueEnd - i - 1);
    }

    return result;
}

void KDController::SetRegisters(_In_ unsigned processorNumber, _In_ const std::map<std::string, AddressType> &registerValues)
{
    for (auto kv : registerValues)
    {
        char command[256];
        if (processorNumber == -1)
            _snprintf_s(command, _TRUNCATE, "r %s=%I64x ; .echo", kv.first.c_str(), kv.second);
        else
            _snprintf_s(command, _TRUNCATE, "%dr %s=%I64x ; .echo", processorNumber, kv.first.c_str(), kv.second);

        std::string reply = ExecuteCommand(command);
        UNREFERENCED_PARAMETER(reply);
    }
}

SimpleCharBuffer KDController::ReadMemory(_In_ AddressType address, _In_ size_t size)
{
    char command[128];
    assert(address <= (address + size));
    _snprintf_s(command, _TRUNCATE, "db %I64x %I64x", address, address + size - 1);

    SimpleCharBuffer result;
    if (!result.TryEnsureCapacity(size))
    {
        throw _com_error(E_OUTOFMEMORY);
    }

    std::string reply = ExecuteCommand(command);
    //Iterate over each line of the reply
    size_t lineStart = 0;
    for (;;)
    {
        //Each line has a format <addr>  <byte values>  <character values>
        size_t lineEnd = reply.find('\n', lineStart);
        size_t addressEnd = reply.find("  ", lineStart);
        if (addressEnd >= lineEnd)
        {
            break;
        }

        size_t byteDumpEnd = reply.find("  ", addressEnd + 1);
        if (addressEnd >= lineEnd)
        {
            break;
        }

        //TODO: check that the reported address matches the expected address

        //Iterate over all 'xx ' items (e.g. 01 02 03 ff)
        for (size_t i = addressEnd + 2; i <= (byteDumpEnd - 2); i+=3)
        {
            if (reply[i + 2] != ' ' && reply[i + 2] != '-')
            {
                throw _com_error(E_FAIL);
            }

            int value = 0;
            if (sscanf_s(reply.c_str() + i, "%x", &value) != 1)
            {
				if (reply[i] == '?')
				{
					//We've reached the end of a mapped page. Partial read here should succeed.
					break;
				}
                throw _com_error(E_FAIL);
            }

            result.SetLength(result.GetLength() + 1);
            result[result.GetLength() - 1] = static_cast<char>(value);

            if (result.GetLength() >= size)
            {
                return result;
            }
        }

        lineStart = lineEnd + 1;

        if (lineEnd == std::string::npos)
        {
            break;
        }
    }
    return result;
}

ULONGLONG KDController::ParseRegisterValue(_In_ const std::string &stringValue)
{
    size_t separatorIndex = stringValue.find('`');
    ULARGE_INTEGER result;
    if (separatorIndex == std::string::npos)
    {
        if (sscanf_s(stringValue.c_str(), "%I64x", &result.QuadPart) != 1)
        {
            throw _com_error(E_INVALIDARG);
        }
    }
    else
    {
        char unused;
        if (sscanf_s(stringValue.c_str(), "%x%c%x", &result.HighPart, &unused, 1, &result.LowPart) != 3)
        {
            throw _com_error(E_INVALIDARG);
        }
    }
    return result.QuadPart;
}

unsigned KDController::GetProcessorCount()
{
	if (m_cachedProcessorCount == 0)
	{
		unsigned greatestID = 0;

		std::string reply = ExecuteCommand("!cpuid");
		//Sample output:
		//CP Model Revision   Manufacturer     MHz
		// 0  XXX   YYY        ZZZ              1234
		// ...
		//Processors are numbered 0 to N-1 where N is the amount of them


		//We are interested in the greatest ID, i.e. the greatest number after the newline
	    //Iterate over all occurances of the newline character in the string
	    for(size_t i = reply.find('\n'); i != std::string::npos; i = reply.find('\n', i + 1))
		{
			size_t nonSpaceCharacter = reply.find_first_not_of(' ', i + 1);
			if (nonSpaceCharacter == std::string::npos)
			{
				continue;
			}
			if (reply[nonSpaceCharacter] < '0' || reply[nonSpaceCharacter] > '9')
			{
				//The first character in the line after spaces is not a valid digit, i.e. this is not a valid ID.
				continue;
			}

			size_t nextSpace = reply.find(' ', nonSpaceCharacter);
			if (nextSpace == std::string::npos)
			{
				//The number is not followed by a space. Skip this line.
				continue;
			}

			size_t idLength = nextSpace - nonSpaceCharacter;
			char id[5] = {0, };
			if (idLength >= sizeof(id))
			{
				//CPU ID greater than 9999? Most likely something went wrong.
				continue;
			}

			memcpy(id, reply.c_str() + nonSpaceCharacter, idLength);

			unsigned parsedID = static_cast<unsigned>(atoi(id));
			if (parsedID > greatestID)
			{
				greatestID = parsedID;
			}
		}

		m_cachedProcessorCount = greatestID + 1;
	}
	return m_cachedProcessorCount;
}

KDController::AddressType KDController::GetKPCRAddress(_In_ unsigned processorNumber)
{
	char command[32] = "!pcr";
	if (processorNumber != -1)
	{
		_snprintf_s(command, _TRUNCATE, "!pcr %d", processorNumber);
	}

	std::string reply = ExecuteCommand(command);

	//As the kd.exe we control does not load any symbols, most likely it will reply 'Unable to read the PCR at xxxx' due to missing symbols.
	//The address contained in the message is exactly what we need.
	static const char unableToReadMessage[] = "Unable to read the PCR at ";
	static const char kpcrMessage[] = "KPCR for Processor ";
	static const char atMessage[] = " at ";

	unsigned unableToReadOffset = static_cast<unsigned>(reply.find(unableToReadMessage));
	unsigned kpcrOffset = static_cast<unsigned>(reply.find(kpcrMessage));

	size_t addressOffset = std::string::npos;

	if (unableToReadOffset < kpcrOffset)
	{
		assert(unableToReadOffset != std::string::npos);
		addressOffset = unableToReadOffset + sizeof(unableToReadMessage) - 1;
	}
	else if (kpcrOffset != std::string::npos)
	{
		size_t atOffset = reply.find(atMessage, kpcrOffset);
		if (atOffset != -1)
		{
			addressOffset = atOffset + sizeof(atMessage) - 1;
		}
	}

	AddressType result = 0;

	if (addressOffset != std::string::npos)
	{
		size_t endOfAddress = reply.find_first_of(" \r\n:", addressOffset);
		if (endOfAddress == -1)
			endOfAddress = reply.length();

		result = ParseRegisterValue(reply.substr(addressOffset, endOfAddress - addressOffset));
	}

	return result;
}

std::string KDController::GetEffectiveMachine(_Out_opt_ std::string * pTargetResponse)
{
    std::string reply = ExecuteCommand(".effmach");
    static const char replyPrefix[] = "Effective machine: ";
    size_t prefixIndex = reply.find(replyPrefix);
    if (prefixIndex == std::string::npos)
    {
        throw std::exception(("Unexpected .effmach reply: " + reply).c_str());
    }

    size_t machineNameIndex = prefixIndex + _countof(replyPrefix) - 1;

    size_t spaceIndex = reply.find(' ', machineNameIndex);
    if (spaceIndex == std::string::npos)
        return reply.substr(machineNameIndex);
    else
    {
        if (pTargetResponse != nullptr)
        {
            pTargetResponse->assign(reply.substr(machineNameIndex, spaceIndex));
        }
        return reply.substr(machineNameIndex, spaceIndex - machineNameIndex);
    }
}
