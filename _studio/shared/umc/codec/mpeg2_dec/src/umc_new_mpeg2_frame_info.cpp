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

#include "umc_new_mpeg2_frame_info.h"

namespace UMC_MPEG2_DECODER
{

bool MPEG2DecoderFrameInfo::IsCompleted() const
{
    if (GetStatus() == MPEG2DecoderFrameInfo::STATUS_COMPLETED)
        return true;

    return false;
}

void MPEG2DecoderFrameInfo::Reset()
{
    Free();

    m_isIntraAU = true;

    m_nextAU = 0;
    m_prevAU = 0;
    m_refAU = 0;

    m_Status = STATUS_NONE;
    m_prepared = 0;
    m_IsIDR = false;

    if (m_sps)
    {
        m_sps->DecrementReference();
        m_sps = 0;
    }
}

void MPEG2DecoderFrameInfo::Free()
{
    size_t count = m_pSliceQueue.size();
    for (size_t i = 0; i < count; i ++)
    {
        MPEG2Slice * pCurSlice = m_pSliceQueue[i];
        pCurSlice->Release();
        pCurSlice->DecrementReference();
    }

    m_SliceCount = 0;

    m_pSliceQueue.clear();
    m_prepared = 0;
}

void MPEG2DecoderFrameInfo::RemoveSlice(int32_t num)
{
    MPEG2Slice * pCurSlice = GetSlice(num);

    if (!pCurSlice) // nothing to do
        return;

    for (int32_t i = num; i < m_SliceCount - 1; i++)
    {
        m_pSliceQueue[i] = m_pSliceQueue[i + 1];
    }

    m_SliceCount--;
    m_pSliceQueue[m_SliceCount] = pCurSlice;
}


} // namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
