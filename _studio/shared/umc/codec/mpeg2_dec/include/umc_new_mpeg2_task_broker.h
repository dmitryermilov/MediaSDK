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

#ifndef __UMC_MPEG2_TASK_BROKER_H
#define __UMC_MPEG2_TASK_BROKER_H

#include <vector>
#include <list>

#include "umc_new_mpeg2_dec_defs.h"
#include "umc_new_mpeg2_heap.h"
#include "umc_new_mpeg2_segment_decoder_base.h"

namespace UMC_MPEG2_DECODER
{
class MPEG2DecoderFrameInfo;
class MPEG2DecoderFrameList;
class MPEG2Slice;
class TaskSupplier_MPEG2;

class DecodingContext;
struct TileThreadingInfo;

class MPEG2Task;

// Decoder task scheduler class
class TaskBroker_MPEG2
{
public:
    TaskBroker_MPEG2(TaskSupplier_MPEG2 * pTaskSupplier);

    // Initialize task broker with threads number
    virtual bool Init(int32_t iConsumerNumber);
    virtual ~TaskBroker_MPEG2();

    // Add frame to decoding queue
    virtual bool AddFrameToDecoding(MPEG2DecoderFrame * pFrame);

    // Returns whether enough bitstream data is evailable to start an asynchronous task
    virtual bool IsEnoughForStartDecoding(bool force);
    // Returns whether there is some work available for specified frame
    bool IsExistTasks(MPEG2DecoderFrame * frame);

    // Tries to find a new task for asynchronous processing
    virtual bool GetNextTask(MPEG2Task *pTask);

    // Reset to default values, stop all activity
    virtual void Reset();
    // Release resources
    virtual void Release();

    // Calculate frame state after a task has been finished
    virtual void AddPerformedTask(MPEG2Task *pTask);

    // Wakes up working threads to start working on new tasks
    virtual void Start();

    // Check whether frame is prepared
    virtual bool PrepareFrame(MPEG2DecoderFrame * pFrame);

    // Lock synchronization mutex
    void Lock();
    // Unlock synchronization mutex
    void Unlock();

    TaskSupplier_MPEG2 * m_pTaskSupplier;

protected:

    // Returns number of access units available in the list but not processed yet
    int32_t GetNumberOfTasks(void);
    // Returns whether frame decoding is finished
    bool IsFrameCompleted(MPEG2DecoderFrame * pFrame) const;

    virtual bool GetNextTaskInternal(MPEG2Task *)
    {
        return false;
    }

    // Try to find an access unit which to decode next
    void InitAUs();
    // Find an access unit which has all slices found
    MPEG2DecoderFrameInfo * FindAU();
    void SwitchCurrentAU();
    // Finish frame decoding
    virtual void CompleteFrame(MPEG2DecoderFrame * frame);
    // Remove access unit from the linked list of frames
    void RemoveAU(MPEG2DecoderFrameInfo * toRemove);

    int32_t m_iConsumerNumber;

    MPEG2DecoderFrameInfo * m_FirstAU;

    bool m_IsShouldQuit;

    typedef std::list<MPEG2DecoderFrame *> FrameQueue;
    FrameQueue m_decodingQueue;
    FrameQueue m_completedQueue;

    UMC::Mutex m_mGuard;
};


} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_TASK_BROKER_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
