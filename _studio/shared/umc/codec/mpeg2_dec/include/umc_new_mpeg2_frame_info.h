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

#ifndef __UMC_MPEG2_FRAME_INFO_H
#define __UMC_MPEG2_FRAME_INFO_H

#include <vector>
#include "umc_new_mpeg2_frame.h"
#include "umc_new_mpeg2_slice_decoding.h"

namespace UMC_MPEG2_DECODER
{
class MPEG2DecoderFrame;


// Collection of slices that constitute one decoder frame
class MPEG2DecoderFrameInfo
{
public:
    enum FillnessStatus
    {
        STATUS_NONE,
        STATUS_NOT_FILLED,
        STATUS_FILLED,
        STATUS_COMPLETED,
        STATUS_STARTED
    };

    MPEG2DecoderFrameInfo(MPEG2DecoderFrame * pFrame, Heap_Objects * pObjHeap)
        : m_pFrame(pFrame)
        , m_prepared(0)
        , m_sps(0)
        , m_SliceCount(0)
        , m_pObjHeap(pObjHeap)
    {
        Reset();
    }

    virtual ~MPEG2DecoderFrameInfo()
    {
    }

    MPEG2Slice *GetAnySlice() const
    {
        MPEG2Slice* slice = GetSlice(0);
        if (!slice)
            throw mpeg2_exception(UMC::UMC_ERR_FAILED);
        return slice;
    }

    const MPEG2SeqParamSet *GetSeqParam() const
    {
        return m_sps;
    }

    void AddSlice(MPEG2Slice * pSlice)
    {
        m_pSliceQueue.push_back(pSlice);
        m_SliceCount++;

        const MPEG2SliceHeader &sliceHeader = *(pSlice->GetSliceHeader());
    }

    uint32_t GetSliceCount() const
    {
        return m_SliceCount;
    }

    MPEG2Slice* GetSlice(int32_t num) const
    {
        if (num < 0 || num >= m_SliceCount)
            return 0;
        return m_pSliceQueue[num];
    }

    MPEG2Slice* GetSliceByNumber(int32_t num) const
    {
        size_t count = m_pSliceQueue.size();
        for (uint32_t i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return m_pSliceQueue[i];
        }

        return 0;
    }

    int32_t GetPositionByNumber(int32_t num) const
    {
        size_t count = m_pSliceQueue.size();
        for (uint32_t i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return i;
        }

        return -1;
    }

    void Reset();

    void SetStatus(FillnessStatus status)
    {
        m_Status = status;
    }

    FillnessStatus GetStatus () const
    {
        return m_Status;
    }

    void Free();
    void RemoveSlice(int32_t num);

    bool IsCompleted() const;

    bool IsIntraAU() const
    {
        return m_isIntraAU;
    }

    bool IsReference() const
    {
        return m_pFrame->m_isUsedAsReference;
    }

    MPEG2DecoderFrameInfo * GetNextAU() const {return m_nextAU;}
    MPEG2DecoderFrameInfo * GetPrevAU() const {return m_prevAU;}
    MPEG2DecoderFrameInfo * GetRefAU() const {return m_refAU;}

    void SetNextAU(MPEG2DecoderFrameInfo *au) {m_nextAU = au;}
    void SetPrevAU(MPEG2DecoderFrameInfo *au) {m_prevAU = au;}
    void SetRefAU(MPEG2DecoderFrameInfo *au) {m_refAU = au;}

    MPEG2DecoderFrame * m_pFrame;
    int32_t m_prepared;
    bool m_IsIDR;

private:

    FillnessStatus m_Status;

    MPEG2SeqParamSet * m_sps;
    std::vector<MPEG2Slice*> m_pSliceQueue;

    int32_t m_SliceCount;

    Heap_Objects * m_pObjHeap;

    bool m_isIntraAU;

    MPEG2DecoderFrameInfo *m_nextAU;
    MPEG2DecoderFrameInfo *m_prevAU;
    MPEG2DecoderFrameInfo *m_refAU;
};

} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_FRAME_INFO_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
