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
#include "umc_structures.h"

#include "umc_new_mpeg2_segment_decoder_dxva.h"
#include "umc_new_mpeg2_frame_list.h"

#include "umc_new_mpeg2_slice_decoding.h"
#include "umc_new_mpeg2_task_supplier.h"
#include "umc_new_mpeg2_frame_info.h"

#include "umc_new_mpeg2_va_packer.h"

#include "mfx_common.h" //  for trace routines

#include "vm_time.h"



namespace UMC_MPEG2_DECODER
{
MPEG2_DXVA_SegmentDecoderCommon::MPEG2_DXVA_SegmentDecoderCommon(TaskSupplier_MPEG2 * pTaskSupplier)
    : MPEG2SegmentDecoderBase(pTaskSupplier->GetTaskBroker())
    , m_va(0)
    , m_pTaskSupplier(pTaskSupplier)
{
}

void MPEG2_DXVA_SegmentDecoderCommon::SetVideoAccelerator(UMC::VideoAccelerator *va)
{
    VM_ASSERT(va);
    m_va = (UMC::VideoAccelerator*)va;
}

MPEG2_DXVA_SegmentDecoder::MPEG2_DXVA_SegmentDecoder(TaskSupplier_MPEG2 * pTaskSupplier)
    : MPEG2_DXVA_SegmentDecoderCommon(pTaskSupplier)
    , m_CurrentSliceID(0)
{
}

MPEG2_DXVA_SegmentDecoder::~MPEG2_DXVA_SegmentDecoder()
{
}

UMC::Status MPEG2_DXVA_SegmentDecoder::Init(int32_t iNumber)
{
    return MPEG2SegmentDecoderBase::Init(iNumber);
}

void MPEG2_DXVA_SegmentDecoder::PackAllHeaders(MPEG2DecoderFrame * pFrame)
{
    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va));
        VM_ASSERT(m_Packer.get());
    }

    m_Packer->BeginFrame(pFrame);
    m_Packer->PackAU(pFrame, m_pTaskSupplier);
    m_Packer->EndFrame();
}

UMC::Status MPEG2_DXVA_SegmentDecoder::ProcessSegment(void)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2_DXVA_SegmentDecoder::ProcessSegment");
    try
    {
        if (!m_pTaskBroker->GetNextTask(0))
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }
    catch (mpeg2_exception const& e)
    {
        return e.GetStatus();
    }

    return UMC::UMC_OK;
}


TaskBrokerSingleThreadDXVA::TaskBrokerSingleThreadDXVA(TaskSupplier_MPEG2 * pTaskSupplier)
    : TaskBroker_MPEG2(pTaskSupplier)
    , m_lastCounter(0)
{
    m_counterFrequency = vm_time_get_frequency();
}

bool TaskBrokerSingleThreadDXVA::PrepareFrame(MPEG2DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->prepared)
    {
        return true;
    }

    if (!pFrame->prepared &&
        (pFrame->GetAU()->GetStatus() == MPEG2DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == MPEG2DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared = true;
    }

    return true;
}

void TaskBrokerSingleThreadDXVA::Reset()
{
    m_lastCounter = 0;
    TaskBroker_MPEG2::Reset();
    m_reports.clear();
}

void TaskBrokerSingleThreadDXVA::Start()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBroker_MPEG2::Start();
    m_completedQueue.clear();
}

enum
{
    NUMBER_OF_STATUS = 512,
};

bool TaskBrokerSingleThreadDXVA::GetNextTaskInternal(MPEG2Task *)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskBrokerSingleThreadDXVA::GetNextTaskInternal");
    UMC::AutomaticUMCMutex guard(m_mGuard);

    // check error(s)
    if (m_IsShouldQuit)
    {
        return false;
    }

    MPEG2_DXVA_SegmentDecoder * dxva_sd = static_cast<MPEG2_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (!dxva_sd->GetPacker())
        return false;

    UMC::Status sts = UMC::UMC_OK;
    VAStatus surfErr = VA_STATUS_SUCCESS;
    int32_t index;

    for (MPEG2DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        index = au->m_pFrame->GetFrameMID();

        m_mGuard.Unlock();
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec vaSyncSurface");
            sts = dxva_sd->GetPacker()->SyncTask(index, &surfErr);
        }
        m_mGuard.Lock();

        //we should complete frame even we got an error
        //this allows to return the error from [RunDecoding]
        au->SetStatus(MPEG2DecoderFrameInfo::STATUS_COMPLETED);
        CompleteFrame(au->m_pFrame);

        if (sts < UMC::UMC_OK)
        {
            if (sts != UMC::UMC_ERR_GPU_HANG)
                sts = UMC::UMC_ERR_DEVICE_FAILED;

            au->m_pFrame->SetError(sts);
        }
        else if (sts == UMC::UMC_OK)
            switch (surfErr)
            {
                case MFX_CORRUPTION_MAJOR:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                    break;

                case MFX_CORRUPTION_MINOR:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                    break;
            }

        if (sts != UMC::UMC_OK)
            throw mpeg2_exception(sts);
    }

    SwitchCurrentAU();

    return false;
}

void TaskBrokerSingleThreadDXVA::AwakeThreads()
{
}

} // namespace UMC_MPEG2_DECODER

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
