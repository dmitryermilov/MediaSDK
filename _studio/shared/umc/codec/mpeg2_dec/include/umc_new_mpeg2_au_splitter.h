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

#ifndef __UMC_MPEG2_AU_SPLITTER_H
#define __UMC_MPEG2_AU_SPLITTER_H

#include <vector>
#include "umc_new_mpeg2_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_new_mpeg2_heap.h"
#include "umc_new_mpeg2_headers.h"
#include "umc_video_decoder.h"

namespace UMC_MPEG2_DECODER
{

class NALUnitSplitter_MPEG2;

// NAL unit splitter wrapper class
class AU_Splitter_MPEG2
{
public:
    AU_Splitter_MPEG2();
    virtual ~AU_Splitter_MPEG2();

    void Init(UMC::VideoDecoderParams *init);
    void Close();

    void Reset();

    // Wrapper for NAL unit splitter CheckNalUnitType
    int32_t CheckNalUnitType(UMC::MediaData * pSource);

    // Wrapper for NAL unit splitter CheckNalUnitType GetNalUnit
    UMC::MediaDataEx * GetNalUnit(UMC::MediaData * src);
    // Returns internal NAL unit splitter
    NALUnitSplitter_MPEG2 * GetNalUnitSplitter();

protected:

    Heap_Objects    m_ObjHeap;
    Headers         m_Headers;

protected:

    std::unique_ptr<NALUnitSplitter_MPEG2> m_pNALSplitter;
};

} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_AU_SPLITTER_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
