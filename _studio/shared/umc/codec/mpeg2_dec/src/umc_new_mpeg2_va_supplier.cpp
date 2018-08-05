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

#include "umc_new_mpeg2_bitstream_headers.h"
#include "umc_new_mpeg2_va_supplier.h"
#include "umc_new_mpeg2_frame_list.h"

#include "umc_new_mpeg2_dec_defs.h"

#include "umc_new_mpeg2_task_broker.h"
#include "umc_structures.h"

#include "umc_new_mpeg2_debug.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"

namespace UMC_MPEG2_DECODER
{

VATaskSupplier::VATaskSupplier()
    : m_bufferedFrameNumber(0)
{
}

UMC::Status VATaskSupplier::Init(UMC::VideoDecoderParams *pInit)
{
    SetVideoHardwareAccelerator(pInit->pVideoAccelerator);
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    pInit->numThreads = 1;

    UMC::Status umsRes = TaskSupplier_MPEG2::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;
    m_iThreadNum = 1;

    DXVASupport<VATaskSupplier>::Init();

    if (m_va)
    {
        m_DPBSizeEx = m_iThreadNum + pInit->info.bitrate;
    }

    m_sei_messages = new SEI_Storer_MPEG2();
    m_sei_messages->Init();

    return UMC::UMC_OK;
}

void VATaskSupplier::CreateTaskBroker()
{
    m_pTaskBroker = new TaskBrokerSingleThreadDXVA(this);

    for (uint32_t i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new MPEG2_DXVA_SegmentDecoder(this);
    }
}

void VATaskSupplier::SetBufferedFramesNumber(uint32_t buffered)
{
    m_DPBSizeEx = 1 + buffered;
    m_bufferedFrameNumber = buffered;
}

MPEG2DecoderFrame * VATaskSupplier::GetFrameToDisplayInternal(bool force)
{
    //ViewItem_MPEG2 &view = *GetView();
    //view.maxDecFrameBuffering += m_bufferedFrameNumber;

    MPEG2DecoderFrame * frame = MFXTaskSupplier_MPEG2::GetFrameToDisplayInternal(force);

    //view.maxDecFrameBuffering -= m_bufferedFrameNumber;

    return frame;
}

void VATaskSupplier::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    MFXTaskSupplier_MPEG2::Reset();
}

inline bool isFreeFrame(MPEG2DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef &&
        !pTmp->m_isLongTermRef
        //((pTmp->m_wasOutputted != 0) || (pTmp->m_Flags.isDisplayable == 0)) &&
        //!pTmp->m_BusyState
        );
}

void VATaskSupplier::CompleteFrame(MPEG2DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    if (pFrame->GetAU()->GetStatus() > MPEG2DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    MFXTaskSupplier_MPEG2::CompleteFrame(pFrame);

    if (MPEG2DecoderFrameInfo::STATUS_FILLED != pFrame->GetAU()->GetStatus())
        return;

    StartDecodingFrame(pFrame);
    EndDecodingFrame();
}

void VATaskSupplier::InitFrameCounter(MPEG2DecoderFrame * pFrame, const MPEG2Slice *pSlice)
{
    TaskSupplier_MPEG2::InitFrameCounter(pFrame, pSlice);
}

UMC::Status VATaskSupplier::AllocateFrameData(MPEG2DecoderFrame * pFrame, mfxSize dimensions, const MPEG2SeqParamSet* pSeqParamSet, const MPEG2PicParamSet *)
{
    UMC::ColorFormat chroma_format_idc = pFrame->GetColorFormat();
    UMC::VideoDataInfo info;
    int32_t bit_depth = pSeqParamSet->need16bitOutput ? 10 : 8;
    info.Init(dimensions.width, dimensions.height, chroma_format_idc, bit_depth);

    UMC::FrameMemID frmMID;
    UMC::Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC::UMC_OK)
    {
        throw mpeg2_exception(UMC::UMC_ERR_ALLOC);
    }

    UMC::FrameData frmData;
    frmData.Init(&info, frmMID, m_pFrameAllocator);

    mfx_UMC_FrameAllocator* mfx_alloc =
        DynamicCast<mfx_UMC_FrameAllocator>(m_pFrameAllocator);
    if (mfx_alloc)
    {
        mfxFrameSurface1* surface =
            mfx_alloc->GetSurfaceByIndex(frmMID);
        if (!surface)
            throw mpeg2_exception(UMC::UMC_ERR_ALLOC);

    }

    pFrame->allocate(&frmData, &info);
    pFrame->m_index = frmMID;

    return UMC::UMC_OK;
}

MPEG2Slice * VATaskSupplier::DecodeSliceHeader(UMC::MediaDataEx *nalUnit)
{
    size_t dataSize = nalUnit->GetDataSize();
    nalUnit->SetDataSize(MFX_MIN(1024, dataSize));

    MPEG2Slice * slice = TaskSupplier_MPEG2::DecodeSliceHeader(nalUnit);

    nalUnit->SetDataSize(dataSize);

    if (!slice)
        return 0;

    if (nalUnit->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
    {
        slice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
        MFX_INTERNAL_CPY(slice->m_source.GetPointer(), nalUnit->GetDataPointer(), (uint32_t)nalUnit->GetDataSize());
        memset(slice->m_source.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);
        slice->m_source.SetDataSize(nalUnit->GetDataSize());
        slice->m_source.SetTime(nalUnit->GetTime());
    }
    else
    {
        slice->m_source.SetData(nalUnit);
    }

    uint32_t* pbs;
    uint32_t bitOffset;

    slice->GetBitStream()->GetState(&pbs, &bitOffset);

    size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();

    slice->GetBitStream()->Reset(slice->m_source.GetPointer(), bitOffset,
        (uint32_t)slice->m_source.GetDataSize());
    slice->GetBitStream()->SetState((uint32_t*)(slice->m_source.GetPointer() + bytes), bitOffset);


    return slice;
}

} // namespace UMC_MPEG2_DECODER

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
