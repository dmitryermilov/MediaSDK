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

#ifndef __UMC_MPEG2_FRAME_LIST_H__
#define __UMC_MPEG2_FRAME_LIST_H__

#include "umc_new_mpeg2_frame.h"

namespace UMC_MPEG2_DECODER
{

class MPEG2DecoderFrameList
{
public:
    // Default constructor
    MPEG2DecoderFrameList(void);
    // Destructor
    virtual
    ~MPEG2DecoderFrameList(void);

    MPEG2DecoderFrame   *head() { return m_pHead; }
    MPEG2DecoderFrame   *tail() { return m_pTail; }

    const MPEG2DecoderFrame   *head() const { return m_pHead; }
    const MPEG2DecoderFrame   *tail() const { return m_pTail; }

    bool isEmpty() { return !m_pHead; }

    void append(MPEG2DecoderFrame *pFrame);
    // Append the given frame to our tail

    int32_t GetFreeIndex()
    {
        for(int32_t i = 0; i < 128; i++)
        {
            MPEG2DecoderFrame *pFrm;

            for (pFrm = head(); pFrm && pFrm->m_index != i; pFrm = pFrm->future())
            {}

            if(pFrm == NULL)
            {
                return i;
            }
        }

        VM_ASSERT(false);
        return -1;
    };

protected:

    // Release object
    void Release(void);

    MPEG2DecoderFrame *m_pHead;                          // (MPEG2DecoderFrame *) pointer to first frame in list
    MPEG2DecoderFrame *m_pTail;                          // (MPEG2DecoderFrame *) pointer to last frame in list
};

class MPEG2DBPList : public MPEG2DecoderFrameList
{
public:

    MPEG2DBPList();

    // Searches DPB for a reusable frame with biggest POC
    MPEG2DecoderFrame * GetOldestDisposable();

    // Returns whether DPB contains frames which may be reused
    bool IsDisposableExist();

    // Returns whether DPB contains frames which may be reused after asynchronous decoding finishes
    bool IsAlmostDisposableExist();

    // Returns first reusable frame in DPB
    MPEG2DecoderFrame *GetDisposable(void);

    // Marks all frames as not used as reference frames.
    void removeAllRef();

    // Increase ref pic list reset count except for one frame
    void IncreaseRefPicListResetCount(MPEG2DecoderFrame *excludeFrame);

    // Searches DPB for a short term reference frame with specified POC
    MPEG2DecoderFrame *findShortRefPic(int32_t picPOC);

    // Searches DPB for a long term reference frame with specified POC
    MPEG2DecoderFrame *findLongTermRefPic(const MPEG2DecoderFrame *excludeFrame, int32_t picPOC, uint32_t bitsForPOC, bool isUseMask) const;

    // Returns the number of frames in DPB
    uint32_t countAllFrames();

    // Return number of active short and long term reference frames.
    void countActiveRefs(uint32_t &numShortTerm, uint32_t &numLongTerm);

    // Search through the list for the oldest displayable frame.
    MPEG2DecoderFrame *findOldestDisplayable(int32_t dbpSize);

    void calculateInfoForDisplay(uint32_t &countDisplayable, uint32_t &countDPBFullness, int32_t &maxUID);

    // Try to find a frame closest to specified for error recovery
    MPEG2DecoderFrame * FindClosest(MPEG2DecoderFrame * pFrame);

    int32_t GetDPBSize() const
    {
        return m_dpbSize;
    }

    void SetDPBSize(int32_t dpbSize)
    {
        m_dpbSize = dpbSize;
    }

    // Reset the buffer and reset every single frame of it
    void Reset(void);

    // Debug print
    void DebugPrint();
    // Debug print
    void printDPB();

protected:
    int32_t m_dpbSize;
};

} // end namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_FRAME_LIST_H__
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
