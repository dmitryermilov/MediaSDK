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

#ifndef __UMC_MPEG2_FRAME_H__
#define __UMC_MPEG2_FRAME_H__

#include <stdlib.h>
#include "umc_new_mpeg2_yuv.h"
#include "umc_new_mpeg2_notify.h"
#include "umc_new_mpeg2_heap.h"


namespace UMC_MPEG2_DECODER
{
class MPEG2Slice;
class MPEG2DecoderFrameInfo;
class MPEG2CodingUnit;

// MPEG2 decoder frame class
class MPEG2DecoderFrame : public MPEG2DecYUVBufferPadded, public RefCounter
{
public:

    int32_t  m_PicOrderCnt;    // Display order picture count mod MAX_PIC_ORDER_CNT.

    int32_t  m_frameOrder;
    uint32_t m_decOrder;
    int32_t m_displayOrder;
    int32_t  m_ErrorType;

    MPEG2DecoderFrameInfo * m_pSlicesInfo;

    bool prepared;

    MPEG2DecoderFrameInfo * GetAU() const
    {
        return m_pSlicesInfo;
    }

    MPEG2DecoderFrame *m_pPreviousFrame;
    MPEG2DecoderFrame *m_pFutureFrame;

    MPEG2SEIPayLoad m_UserData;

    double           m_dFrameTime;
    bool             m_isOriginalPTS;

    DisplayPictureStruct_MPEG2  m_DisplayPictureStruct_MPEG2;

    // For type 1 calculation of m_PicOrderCnt. m_FrameNum is needed to
    // be used as previous frame num.

    bool post_procces_complete;

    int32_t m_index;
    int32_t m_UID;
    UMC::FrameType m_FrameType;

    UMC::MemID m_MemID;

    int32_t           m_RefPicListResetCount;
    int32_t           m_crop_left;
    int32_t           m_crop_right;
    int32_t           m_crop_top;
    int32_t           m_crop_bottom;
    int32_t           m_crop_flag;

    int32_t           m_aspect_width;
    int32_t           m_aspect_height;

    bool              m_pic_output;

    bool              m_isShortTermRef;
    bool              m_isLongTermRef;
    bool              m_isUsedAsReference;

    // Returns whether frame has all slices found
    bool IsFullFrame() const;
    // Set frame flag denoting that all slices for it were found
    void SetFullFrame(bool isFull);

    struct
    {
        uint8_t  isFull    : 1;
        uint8_t  isDecoded : 1;
        uint8_t  isDecodingStarted : 1;
        uint8_t  isDecodingCompleted : 1;
    } m_Flags;

    uint8_t  m_isDisplayable;
    uint8_t  m_wasDisplayed;
    uint8_t  m_wasOutputted;
    int32_t  m_maxUIDWhenWasDisplayed;

    typedef std::list<RefCounter *>  ReferenceList;
    ReferenceList m_references;

    // Add target frame to the list of reference frames
    void AddReferenceFrame(MPEG2DecoderFrame * frm);
    // Clear all references to other frames
    void FreeReferenceFrames();

    // Reinitialize frame structure before reusing frame
    void Reset();
    // Clean up data after decoding is done
    void FreeResources();

    // Accelerator for getting 'surface Index' FrameMID
    inline int32_t GetFrameMID() const
    {
        return m_frameData.GetFrameMID();
    }
public:
    // Delete unneeded references and set flags after decoding is done
    void OnDecodingCompleted();

    // Free resources if possible
    virtual void Free();

    // Returns whether frame has been decoded
    bool IsDecoded() const;

    MPEG2DecoderFrame(UMC::MemoryAllocator *pMemoryAllocator, Heap_Objects * pObjHeap);

    virtual ~MPEG2DecoderFrame();

    // The following methods provide access to the MPEG2 Decoder's doubly
    // linked list of MPEG2DecoderFrames.  Note that m_pPreviousFrame can
    // be non-NULL even for an I frame.
    MPEG2DecoderFrame *future() const  { return m_pFutureFrame; }

    void setPrevious(MPEG2DecoderFrame *pPrev)
    {
        m_pPreviousFrame = pPrev;
    }

    void setFuture(MPEG2DecoderFrame *pFut)
    {
        m_pFutureFrame = pFut;
    }

    bool        isDisplayable()    { return ((m_Flags.isDecodingStarted != 0) && (m_isDisplayable)); }

    void        SetisDisplayable(bool isDisplayable)
    {
        m_isDisplayable = isDisplayable ? 1 : 0;
    }

    bool IsDecodingStarted() const { return m_Flags.isDecodingStarted != 0;}
    void StartDecoding() { m_Flags.isDecodingStarted = 1;}

    bool IsDecodingCompleted() const { return m_Flags.isDecodingCompleted != 0;}
    // Flag frame as completely decoded
    void CompleteDecoding();

    // Check reference frames for error status and flag this frame if error is found
    void UpdateErrorWithRefFrameStatus();

    bool        wasDisplayed()    { return m_wasDisplayed != 0; }
    void        setWasDisplayed() { m_wasDisplayed = 1; }

    bool        wasOutputted()    { return m_wasOutputted != 0; }
    void        setWasOutputted(); // Flag frame after it was output

    bool        isDisposable()    { return (!m_isShortTermRef &&
                                            !m_isLongTermRef &&
                                            ((m_wasOutputted != 0 && m_wasDisplayed != 0) || (m_isDisplayable == 0)) &&
                                            !GetRefCounter()); }

    bool isShortTermRef() const
    {
        return m_isShortTermRef;
    }

    // Mark frame as short term reference frame
    void SetisShortTermRef(bool isRef);

    int32_t PicOrderCnt() const
    {
        return m_PicOrderCnt;
    }
    void setPicOrderCnt(int32_t PicOrderCnt)
    {
        m_PicOrderCnt = PicOrderCnt;
    }

    bool isLongTermRef() const
    {
        return m_isLongTermRef;
    }

    // Mark frame as long term reference frame
    void SetisLongTermRef(bool isRef);

    void IncreaseRefPicListResetCount()
    {
        m_RefPicListResetCount++;
    }

    void InitRefPicListResetCount()
    {
        m_RefPicListResetCount = 0;
    }

    int32_t RefPicListResetCount() const
    {
        return m_RefPicListResetCount;
    }

    MPEG2_FORCEINLINE const MPEG2DecoderFrame* GetRefPicList(uint8_t direction) const
    {
        return m_refPicList[direction];
    }

    void SetForwardRefPic(MPEG2DecoderFrame * frm)
    {
        AddReferenceFrame(frm);
        m_refPicList[0] = frm;
    }

    void SetBackwardRefPic(MPEG2DecoderFrame * frm)
    {
        AddReferenceFrame(frm);
        m_refPicList[1] = frm;
    }

    int32_t GetError() const
    {
        return m_ErrorType;
    }

    void SetError(int32_t errorType)
    {
        m_ErrorType = errorType;
    }

    void SetErrorFlagged(int32_t errorType)
    {
        m_ErrorType |= errorType;
    }

    void AddSlice(MPEG2Slice * pSlice);

protected:
    Heap_Objects * m_pObjHeap;

    MPEG2DecoderFrame * m_refPicList[2];

    class FakeFrameInitializer
    {
    public:
        FakeFrameInitializer();
    };

    static FakeFrameInitializer g_FakeFrameInitializer;

    bool CheckReferenceFrameError();
};

// Returns if frame is not needed by decoder
inline bool isAlmostDisposable(MPEG2DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef &&
        !pTmp->m_isLongTermRef &&
        ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)) &&
        !pTmp->GetRefCounter());
}

// Returns if frame is not needed by decoder
inline bool isInDisplayngStage(MPEG2DecoderFrame * pTmp)
{
    return (pTmp->m_isDisplayable && pTmp->m_wasOutputted && !pTmp->m_wasDisplayed);
}

} // end namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_FRAME_H__
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
