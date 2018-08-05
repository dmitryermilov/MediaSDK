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

#ifndef __UMC_MPEG2_SEGMENT_DECODER_BASE_H
#define __UMC_MPEG2_SEGMENT_DECODER_BASE_H

#include "umc_new_mpeg2_dec_defs.h"

namespace UMC_MPEG2_DECODER
{

class MPEG2Task;
class TaskBroker_MPEG2;

class MPEG2SegmentDecoderBase
{
public:
    MPEG2SegmentDecoderBase(TaskBroker_MPEG2 * pTaskBroker)
        : m_iNumber(0)
        , m_pTaskBroker(pTaskBroker)
    {
    }

    virtual ~MPEG2SegmentDecoderBase()
    {
    }

    virtual UMC::Status Init(int32_t iNumber)
    {
        m_iNumber = iNumber;
        return UMC::UMC_OK;
    }

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual void RestoreErrorRect(MPEG2Task *)
    {
    }

protected:
    int32_t m_iNumber;                                           // (int32_t) ordinal number of decoder
    TaskBroker_MPEG2 * m_pTaskBroker;
};

} // namespace UMC_MPEG2_DECODER

#endif /* __UMC_MPEG2_SEGMENT_DECODER_BASE_H */
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
