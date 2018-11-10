// Copyright (c) 2017 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#ifdef UMC_ENABLE_MPEG2_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_new_mpeg2_heap.h"
#include "umc_new_mpeg2_dec_defs.h"
#include <cstdarg>

namespace UMC_MPEG2_DECODER
{

    void HeapObject::Free()
{
    Item * item = (Item *) ((uint8_t*)this - sizeof(Item));
    item->m_heap->Free(this);
}

void RefCounter::IncrementReference() const
{
    m_refCounter++;
}

void RefCounter::DecrementReference()
{
    m_refCounter--;

    VM_ASSERT(m_refCounter >= 0);
    if (!m_refCounter)
    {
        Free();
    }
}

// Allocate several arrays inside of one memory buffer
uint8_t * CumulativeArraysAllocation(int n, int align, ...)
{
    va_list args;
    va_start(args, align);

    int cumulativeSize = 0;
    for (int i = 0; i < n; i++)
    {
        void * ptr = va_arg(args, void *);
        ptr; // just skip it

        int currSize = va_arg(args, int);
        cumulativeSize += currSize;
    }

    va_end(args);

    uint8_t *cumulativePtr = mpeg2_new_array_throw<uint8_t>(cumulativeSize + align*n);
    uint8_t *cumulativePtrSaved = cumulativePtr;

    va_start(args, align);

    for (int i = 0; i < n; i++)
    {
        void ** ptr = va_arg(args, void **);

        *ptr = align ? UMC::align_pointer<void*> (cumulativePtr, align) : cumulativePtr;

        int currSize = va_arg(args, int);
        cumulativePtr = (uint8_t*)*ptr + currSize;
    }

    va_end(args);
    return cumulativePtrSaved;
}

// Free memory allocated by CumulativeArraysAllocation
void CumulativeFree(uint8_t * ptr)
{
    delete[] ptr;
}

} // namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
