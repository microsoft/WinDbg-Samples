//----------------------------------------------------------------------------
//
// BufferWrapperTest.cpp
//
// Unit tests for BufferWrapper class.
//
// Copyright (C) Microsoft Corporation, 2013.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "CppUnitTest.h"

#include "../KDControllerLib/BufferWrapper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace KDControllerLib;

namespace KDControllerLibTests
{		
	TEST_CLASS(BufferWrapperTest)
	{
	public:
        typedef int TestElementType;

		TEST_METHOD(TestDefaultState)
		{
			BufferWrapper<TestElementType> buffer;
            Assert::IsTrue(buffer.GetCapacity() == 0);
            Assert::IsTrue(buffer.GetInternalBuffer() == nullptr);
            Assert::IsTrue(buffer.GetLength() == 0);
		}

        TEST_METHOD(TestEnsureCapacity)
        {
            size_t const validAllocationLength = 4096;
            size_t const invalidAllocationLength = (INT_MAX / sizeof(TestElementType));

            BufferWrapper<TestElementType> buffer;
            Assert::IsTrue(buffer.TryEnsureCapacity(0));
            Assert::IsTrue(buffer.GetInternalBuffer() == nullptr);

            Assert::IsTrue(buffer.TryEnsureCapacity(validAllocationLength));
            Assert::IsTrue(buffer.GetInternalBuffer() != nullptr);
            Assert::IsTrue(buffer.GetLength() == 0);
            Assert::IsTrue(buffer.GetCapacity() == validAllocationLength);

            int *pOldData = buffer.GetInternalBuffer();

            Assert::IsFalse(buffer.TryEnsureCapacity(invalidAllocationLength));
            Assert::IsTrue(buffer.GetInternalBuffer() == pOldData);
        }

        TEST_METHOD(TestGetEndOfData)
        {
            size_t const allocationLength = 4096;
            size_t const useLength = 1024;

            BufferWrapper<TestElementType> buffer = CreateBufferWrapper(allocationLength, useLength);
            Assert::IsTrue(buffer.GetEndOfData() == buffer.GetInternalBuffer() + useLength);
        }

        TEST_METHOD(TestIndexingOperator)
        {
            size_t const allocationLength = 4096;
            size_t const useLength = 1024;

            BufferWrapper<TestElementType> buffer = CreateBufferWrapper(allocationLength, useLength);
            for (int i = 0; i < useLength; ++i)
            {
                Assert::IsTrue(&buffer[i] == buffer.GetInternalBuffer() + i);
            }
        }

    private:
        BufferWrapper<TestElementType> CreateBufferWrapper(_In_ size_t allocationLength, _In_ size_t useLength)
        {
            BufferWrapper<TestElementType> buffer;
            Assert::IsTrue(buffer.TryEnsureCapacity(allocationLength));
            Assert::IsTrue(buffer.GetLength() == 0);
            buffer.SetLength(useLength);
            Assert::IsTrue(buffer.GetLength() == useLength);

            return buffer;
        }
	};
}