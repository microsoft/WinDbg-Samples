//----------------------------------------------------------------------------
//
// BufferedStreamReaderTest.cpp
//
// Unit tests for BufferedStreamReader class.
//
// Copyright (C) Microsoft Corporation, 2013.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "CppUnitTest.h"

#include "../KDControllerLib/BufferedStreamReader.h"
#include "../KDControllerLib/ExceptionHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace KDControllerLib;

namespace KdControllerLibTests
{
	TEST_CLASS(BufferedStreamReaderTest)
	{
	public:
        BufferedStreamReaderTest()
            : m_CRLF("\r\n")
        {
        }

        TEST_METHOD(BasicReadTest)
        {
            TemporaryPipe pipe;
            BufferedStreamReader reader(pipe.GetReadHandle());

            pipe.WriteText("line 1\r\nline2\r\nline #3\r\nend\r");
            pipe.CloseWriteHandle();
            Assert::AreEqual(reader.Read(m_CRLF).c_str(), "line 1");
            Assert::AreEqual(reader.Read(m_CRLF).c_str(), "line2");
            Assert::AreEqual(reader.Read(m_CRLF).c_str(), "line #3");
            Assert::ExpectException<_com_error>([&](){ reader.Read(m_CRLF);});
        }

        TEST_METHOD(ReadToEndTest)
        {
            TemporaryPipe pipe;
            BufferedStreamReader reader(pipe.GetReadHandle());

            pipe.WriteText("line 1\r\nline 2\r\n");
            pipe.CloseWriteHandle();
            Assert::AreEqual(reader.Read(m_CRLF).c_str(), "line 1");
            Assert::AreEqual(reader.Read(m_CRLF).c_str(), "line 2");
            Assert::ExpectException<_com_error>([&](){ reader.Read(m_CRLF);});
        }

        TEST_METHOD(RandomizedReadTest)
        {
            DWORD threadId;
            TemporaryPipe pipe;
            HANDLE randomWritingThreadHandle = CreateThread(nullptr, 0, RandomWritingThread, &pipe, 0, &threadId);
            if (randomWritingThreadHandle == nullptr)
			{
                throw std::exception("Cannot create a random writing thread - aborting tests");
			}
            srand(c_randomSeed);

            BufferedStreamReader reader(pipe.GetReadHandle());
            for (int i = 0; i < c_randomTestIterations; ++i)
            {
                char buffer[128];
                _snprintf_s(buffer, _TRUNCATE, "%d", rand());

                Assert::AreEqual(reader.Read(m_CRLF).c_str(), buffer);
            }
         
            Assert::ExpectException<_com_error>([&](){ reader.Read(m_CRLF);});
            
            WaitForSingleObject(randomWritingThreadHandle, INFINITE);
            CloseHandle(randomWritingThreadHandle);
        }

    private:
        static int const c_randomSeed = 123;
        static int const c_randomTestIterations = 1024;
        std::regex m_CRLF;

        class TemporaryPipe
        {
        public:
            TemporaryPipe()
                : m_readHandle(INVALID_HANDLE_VALUE)
                , m_writeHandle(INVALID_HANDLE_VALUE)
            {
                if (!::CreatePipe(&m_readHandle, &m_writeHandle, nullptr, 0))
                {
                    throw std::exception("Cannot create a temporary pipe");            
                }
            }

            ~TemporaryPipe()
            {
                if (m_readHandle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_readHandle);
                }
                if (m_writeHandle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_writeHandle);
                }
            }

            void WriteText(_In_ LPCSTR pText)
            {
                DWORD done;
                if (!WriteFile(m_writeHandle, pText, strlen(pText), &done, nullptr))
                {
                    throw std::exception("Cannot write to temporary pipe");
                }
            }

            HANDLE GetReadHandle()
            {
                return m_readHandle;
            }

            void CloseWriteHandle()
            {
                CloseHandle(m_writeHandle);
                m_writeHandle = INVALID_HANDLE_VALUE;
            }

        private:
            HANDLE m_readHandle;
            HANDLE m_writeHandle;
        };

        static DWORD CALLBACK RandomWritingThread(LPVOID pArgument)
        {
            assert(pArgument != nullptr);
            TemporaryPipe *pPipe = reinterpret_cast<TemporaryPipe *>(pArgument);
            srand(c_randomSeed);

            for (int i = 0; i < c_randomTestIterations; ++i)
            {
                char buffer[128] = "";
                _snprintf_s(buffer, _TRUNCATE, "%d\r\n", rand());

                pPipe->WriteText(buffer);
            }

            pPipe->CloseWriteHandle();            
            return 0;
        }
	};
}