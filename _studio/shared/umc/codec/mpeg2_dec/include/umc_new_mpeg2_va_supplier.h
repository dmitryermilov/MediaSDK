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

#ifndef __UMC_MPEG2_VA_SUPPLIER_H
#define __UMC_MPEG2_VA_SUPPLIER_H

#include "umc_new_mpeg2_mfx_supplier.h"
#include "umc_new_mpeg2_segment_decoder_dxva.h"

namespace UMC_MPEG2_DECODER
{

class MFXVideoDECODEMPEG2;

/****************************************************************************************************/
// TaskSupplier_MPEG2
/****************************************************************************************************/
class VATaskSupplier :
          public MFXTaskSupplier_MPEG2
        , public DXVASupport<VATaskSupplier>
{
    friend class TaskBroker_MPEG2;
    friend class DXVASupport<VATaskSupplier>;
    friend class VideoDECODEMPEG2;

public:

    VATaskSupplier();

    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    virtual void Reset();

    virtual void CreateTaskBroker();

    void SetBufferedFramesNumber(uint32_t buffered);


protected:
    virtual UMC::Status AllocateFrameData(MPEG2DecoderFrame * pFrame, mfxSize dimensions, const MPEG2SeqParamSet* pSeqParamSet, const MPEG2PicParamSet *pPicParamSet);

    virtual void InitFrameCounter(MPEG2DecoderFrame * pFrame, const MPEG2Slice *pSlice);

    virtual void CompleteFrame(MPEG2DecoderFrame * pFrame);

    virtual MPEG2Slice * DecodeSliceHeader(UMC::MediaDataEx *nalUnit);

    virtual MPEG2DecoderFrame *GetFrameToDisplayInternal(bool force);

    uint32_t m_bufferedFrameNumber;

private:
    VATaskSupplier & operator = (VATaskSupplier &)
    {
        return *this;
    }
};

// this template class added to apply big surface pool workaround depends on platform
// because platform check can't be added inside VATaskSupplier
template <class BaseClass>
class VATaskSupplierBigSurfacePool:
    public BaseClass
{
public:
    VATaskSupplierBigSurfacePool()
    {};
    virtual ~VATaskSupplierBigSurfacePool()
    {};

protected:

    virtual UMC::Status AllocateFrameData(MPEG2DecoderFrame * pFrame, mfxSize dimensions, const MPEG2SeqParamSet* pSeqParamSet, const MPEG2PicParamSet * pps)
    {
        UMC::Status ret = BaseClass::AllocateFrameData(pFrame, dimensions, pSeqParamSet, pps);

        if (ret == UMC::UMC_OK)
        {
            ViewItem_MPEG2 *pView = BaseClass::GetView();
            MPEG2DBPList *pDPB = pView->pDPB.get();

            pFrame->m_index = pDPB->GetFreeIndex();
        }

        return ret;
    }
};

}// namespace UMC_MPEG2_DECODER


#endif // __UMC_MPEG2_VA_SUPPLIER_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
