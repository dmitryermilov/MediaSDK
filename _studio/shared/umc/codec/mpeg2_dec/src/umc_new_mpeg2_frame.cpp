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

#include <algorithm>
#include "umc_new_mpeg2_frame.h"
#include "umc_new_mpeg2_task_supplier.h"
#include "umc_new_mpeg2_debug.h"



namespace UMC_MPEG2_DECODER
{

MPEG2DecoderFrame::MPEG2DecoderFrame(UMC::MemoryAllocator *pMemoryAllocator, Heap_Objects * pObjHeap)
    : MPEG2DecYUVBufferPadded(pMemoryAllocator)
    , m_ErrorType(0)
    , m_pSlicesInfo(0)
    , m_pPreviousFrame(0)
    , m_pFutureFrame(0)
    , m_dFrameTime(-1.0)
    , m_isOriginalPTS(false)
    , post_procces_complete(false)
    , m_index(-1)
    , m_UID(-1)
    , m_pObjHeap(pObjHeap)
{
    m_isShortTermRef = false;
    m_isLongTermRef = false;
    m_PicOrderCnt = 0;

    // set memory managment tools
    m_pMemoryAllocator = pMemoryAllocator;

    ResetRefCounter();

    m_pSlicesInfo = new MPEG2DecoderFrameInfo(this, m_pObjHeap);

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;

    m_isDisplayable = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_maxUIDWhenWasDisplayed = 0;
    m_crop_flag = 0;
    m_crop_left = 0;
    m_crop_top = 0;
    m_crop_bottom = 0;
    m_pic_output = 0;
    m_FrameType = UMC::NONE_PICTURE;
    m_aspect_width = 0;
    m_MemID = 0;
    m_aspect_height = 0;
    m_isUsedAsReference = 0;
    m_crop_right = 0;
    m_DisplayPictureStruct_MPEG2 = DPS_FRAME_MPEG2;
    m_frameOrder = 0;
    m_decOrder   = 0;
    m_displayOrder = 0xffffffff;
    prepared = false;
}

MPEG2DecoderFrame::~MPEG2DecoderFrame()
{
    if (m_pSlicesInfo)
    {
        delete m_pSlicesInfo;
        m_pSlicesInfo = 0;
    }

    // Just to be safe.
    m_pPreviousFrame = 0;
    m_pFutureFrame = 0;
    Reset();
    deallocate();
}

// Add target frame to the list of reference frames
void MPEG2DecoderFrame::AddReferenceFrame(MPEG2DecoderFrame * frm)
{
    if (!frm || frm == this)
        return;

    if (std::find(m_references.begin(), m_references.end(), frm) != m_references.end())
        return;

    frm->IncrementReference();
    m_references.push_back(frm);
}

// Clear all references to other frames
void MPEG2DecoderFrame::FreeReferenceFrames()
{
    ReferenceList::iterator iter = m_references.begin();
    ReferenceList::iterator end_iter = m_references.end();

    for (; iter != end_iter; ++iter)
    {
        RefCounter *reference = *iter;
        reference->DecrementReference();
    }

    m_references.clear();
}

// Reinitialize frame structure before reusing frame
void MPEG2DecoderFrame::Reset()
{
    if (m_pSlicesInfo)
        m_pSlicesInfo->Reset();

    ResetRefCounter();

    m_isShortTermRef = false;
    m_isLongTermRef = false;

    post_procces_complete = false;

    m_PicOrderCnt = 0;

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;

    m_isDisplayable = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_pic_output = true;
    m_maxUIDWhenWasDisplayed = 0;

    m_dFrameTime = -1;
    m_isOriginalPTS = false;

    m_isUsedAsReference = false;

    m_UserData.Reset();

    m_ErrorType = 0;
    m_UID = -1;
    m_index = -1;

    m_MemID = MID_INVALID;

    prepared = false;

    FreeReferenceFrames();

    deallocate();

    m_refPicList[0] = nullptr;
    m_refPicList[1] = nullptr;
}

// Returns whether frame has all slices found
bool MPEG2DecoderFrame::IsFullFrame() const
{
    return (m_Flags.isFull == 1);
}

// Set frame flag denoting that all slices for it were found
void MPEG2DecoderFrame::SetFullFrame(bool isFull)
{
    m_Flags.isFull = (uint8_t) (isFull ? 1 : 0);
}

// Returns whether frame has been decoded
bool MPEG2DecoderFrame::IsDecoded() const
{
    return m_Flags.isDecoded == 1;
}

// Clean up data after decoding is done
void MPEG2DecoderFrame::FreeResources()
{
    FreeReferenceFrames();

    if (m_pSlicesInfo && IsDecoded())
        m_pSlicesInfo->Free();
}

// Flag frame as completely decoded
void MPEG2DecoderFrame::CompleteDecoding()
{
    m_Flags.isDecodingCompleted = 1;
    UpdateErrorWithRefFrameStatus();
}

// Check reference frames for error status and flag this frame if error is found
void MPEG2DecoderFrame::UpdateErrorWithRefFrameStatus()
{
    if (CheckReferenceFrameError())
    {
        SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
    }
}

// Delete unneeded references and set flags after decoding is done
void MPEG2DecoderFrame::OnDecodingCompleted()
{
    UpdateErrorWithRefFrameStatus();

    m_Flags.isDecoded = 1;
    DEBUG_PRINT1((VM_STRING("On decoding complete decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
    DecrementReference();

    FreeResources();
}

// Mark frame as short term reference frame
void MPEG2DecoderFrame::SetisShortTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isShortTermRef = true;
    }
    else
    {
        bool wasRef = isShortTermRef() != 0;

        m_isShortTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            DecrementReference();
            DEBUG_PRINT1((VM_STRING("On was short term ref decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
        }
    }
}

// Mark frame as long term reference frame
void MPEG2DecoderFrame::SetisLongTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isLongTermRef = true;
    }
    else
    {
        bool wasRef = isLongTermRef() != 0;

        m_isLongTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            DEBUG_PRINT1((VM_STRING("On was long term reft decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
            DecrementReference();
        }
    }
}

// Flag frame after it was output
void MPEG2DecoderFrame::setWasOutputted()
{
    m_wasOutputted = 1;
}

// Free resources if possible
void MPEG2DecoderFrame::Free()
{
    if (wasDisplayed() && wasOutputted())
        Reset();
}

void MPEG2DecoderFrame::AddSlice(MPEG2Slice * pSlice)
{
    int32_t iSliceNumber = m_pSlicesInfo->GetSliceCount() + 1;

    pSlice->SetSliceNumber(iSliceNumber);
    pSlice->m_pCurrentFrame = this;
    m_pSlicesInfo->AddSlice(pSlice);
}

bool MPEG2DecoderFrame::CheckReferenceFrameError()
{
    uint32_t checkedErrorMask = UMC::ERROR_FRAME_MINOR | UMC::ERROR_FRAME_MAJOR | UMC::ERROR_FRAME_REFERENCE_FRAME;

    if (m_refPicList[0] && m_refPicList[0]->GetError() & checkedErrorMask)
        return true;

    if (m_refPicList[1] && m_refPicList[1]->GetError() & checkedErrorMask)
        return true;

    return false;
}


} // end namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
