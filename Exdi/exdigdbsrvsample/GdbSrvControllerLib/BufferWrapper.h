//----------------------------------------------------------------------------
//
//  BufferWrapper.h
//
//  This is an utility class encapsulating a memory buffer with length and capacity.
//  This class does not throw any exceptions.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once
#include <assert.h>

namespace GdbSrvControllerLib
{
    //  This is a lightweight typed buffer. It encapsulates a pointer and information about how many
    //  elements have been allocated and actually used. The class does not throw any exceptions.
    //NOTE: TElementType should be a plain data type without any constructors. E.g. char or wchar_t.
    template <class TElementType> class BufferWrapper
    {
    public:
         BufferWrapper() 
             : m_pBuffer(nullptr)
             , m_capacity(0)
             , m_length(0)
         {
         }

         ~BufferWrapper() 
         {
             if (m_pBuffer != nullptr)
             {
                 free(m_pBuffer);
             }
         }

         bool TryEnsureCapacity(_In_ size_t newElementCount) 
         {
             if (newElementCount == 0)
             {
                 return true;
             }

             TElementType *pNewBuffer = static_cast<TElementType *>(realloc(m_pBuffer, 
                                                                            newElementCount * sizeof(TElementType)));
             if (pNewBuffer == nullptr)
             {
                 return false;
             }

             m_pBuffer = pNewBuffer;
             m_capacity = newElementCount;
             return true;
         }

         TElementType *GetInternalBuffer() 
         {
             return m_pBuffer;
         }

         TElementType *GetEndOfData() const 
         {
             assert(m_length <= m_capacity);
             return m_pBuffer + m_length;
         }

         size_t GetLength() const 
         {
             return m_length;
         }

         void SetLength(_In_ size_t newLength) 
         {
             assert(newLength <= m_capacity);
             m_length = newLength;
         }

         size_t GetCapacity() const 
         {
             return m_capacity;
         }

         TElementType &operator[](_In_ size_t index) 
         {
             assert(index < m_length);
             assert(m_pBuffer != nullptr);
             return m_pBuffer[index];
         }

		 //C++11 move constructor. See http://msdn.microsoft.com/en-us/library/dd293665.aspx for explanation.
         BufferWrapper(_Inout_ BufferWrapper &&anotherBuffer) 
         {
             m_pBuffer = anotherBuffer.m_pBuffer;
             m_capacity = anotherBuffer.m_capacity;
             m_length = anotherBuffer.m_length;

             anotherBuffer.m_capacity = anotherBuffer.m_length = 0;
             anotherBuffer.m_pBuffer = nullptr;
         }

    private:
        TElementType *m_pBuffer;
        size_t m_capacity;
        size_t m_length;

        BufferWrapper(_In_ const BufferWrapper &anotherBuffer);
        void operator=(_In_  BufferWrapper &anotherBuffer);
    };

    typedef BufferWrapper<char> SimpleCharBuffer;
}