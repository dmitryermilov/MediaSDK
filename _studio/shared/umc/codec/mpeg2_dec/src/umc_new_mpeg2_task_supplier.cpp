// Copyright (c) 2018 Intel Corporation
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

#include "memory"

#include <limits.h> // for INT_MIN, INT_MAX on Linux
#include <functional>
#include <algorithm>

#include "umc_new_mpeg2_task_supplier.h"
#include "umc_new_mpeg2_frame_list.h"
#include "umc_new_mpeg2_nal_spl.h"
#include "umc_new_mpeg2_bitstream_headers.h"

#include "umc_new_mpeg2_dec_defs.h"
#include "vm_sys_info.h"

#include "umc_new_mpeg2_task_broker.h"

#include "umc_structures.h"
#include "umc_frame_data.h"
#include "umc_new_mpeg2_debug.h"


#include "mfx_common.h" //  for trace routines

namespace UMC_MPEG2_DECODER
{

const uint32_t levelIndexArray[] = {
    MPEG2_LEVEL_1,
    MPEG2_LEVEL_2,
    MPEG2_LEVEL_21,

    MPEG2_LEVEL_3,
    MPEG2_LEVEL_31,

    MPEG2_LEVEL_4,
    MPEG2_LEVEL_41,

    MPEG2_LEVEL_5,
    MPEG2_LEVEL_51,
    MPEG2_LEVEL_52,

    MPEG2_LEVEL_6,
    MPEG2_LEVEL_61,
    MPEG2_LEVEL_62
};

/****************************************************************************************************/
// DecReferencePictureMarking_MPEG2
/****************************************************************************************************/
DecReferencePictureMarking_MPEG2::DecReferencePictureMarking_MPEG2()
    : m_isDPBErrorFound(0)
    , m_frameCount(0)
{
}

void DecReferencePictureMarking_MPEG2::Reset()
{
    m_frameCount = 0;
    m_isDPBErrorFound = 0;
}

void DecReferencePictureMarking_MPEG2::ResetError()
{
    m_isDPBErrorFound = 0;
}

uint32_t DecReferencePictureMarking_MPEG2::GetDPBError() const
{
    return m_isDPBErrorFound;
}

// Check if bitstream resolution has changed
static
bool IsNeedSPSInvalidate(const MPEG2SeqParamSet *old_sps, const MPEG2SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->pic_width_in_luma_samples != new_sps->pic_width_in_luma_samples)
        return true;

    if (old_sps->pic_height_in_luma_samples != new_sps->pic_height_in_luma_samples)
        return true;

    if (old_sps->bit_depth_luma != new_sps->bit_depth_luma)
        return true;

    if (old_sps->bit_depth_chroma != new_sps->bit_depth_chroma)
        return true;

    if (old_sps->m_pcPTL.GetGeneralPTL()->profile_idc != new_sps->m_pcPTL.GetGeneralPTL()->profile_idc)
        return true;

    if (old_sps->chroma_format_idc != new_sps->chroma_format_idc)
        return true;

    if (old_sps->sps_max_dec_pic_buffering[0] < new_sps->sps_max_dec_pic_buffering[0])
        return true;

    return false;
}

/****************************************************************************************************/
// MVC_Extension_MPEG2 class routine
/****************************************************************************************************/
MVC_Extension::MVC_Extension()
    : m_temporal_id(7)
    , m_priority_id(63)
    , HighestTid(0)
    , m_level_idc(0)
{
    Reset();
}

MVC_Extension::~MVC_Extension()
{
    Close();
}

UMC::Status MVC_Extension::Init()
{
    MVC_Extension::Close();

    m_view.pDPB.reset(new MPEG2DBPList());
    return UMC::UMC_OK;
}

void MVC_Extension::Close()
{
    MVC_Extension::Reset();
}

void MVC_Extension::Reset()
{
    m_temporal_id = 7;
    m_priority_id = 63;
    m_level_idc = 0;
    HighestTid = 0;

    m_view.Reset();
}

ViewItem_MPEG2 * MVC_Extension::GetView()
{
    return &m_view;
}


/****************************************************************************************************/
// Skipping_MPEG2 class routine
/****************************************************************************************************/
Skipping_MPEG2::Skipping_MPEG2()
    : m_VideoDecodingSpeed(0)
    , m_SkipCycle(1)
    , m_ModSkipCycle(1)
    , m_PermanentTurnOffDeblocking(0)
    , m_SkipFlag(0)
    , m_NumberOfSkippedFrames(0)
{
}

Skipping_MPEG2::~Skipping_MPEG2()
{
}

void Skipping_MPEG2::Reset()
{
    m_VideoDecodingSpeed = 0;
    m_SkipCycle = 0;
    m_ModSkipCycle = 0;
    m_PermanentTurnOffDeblocking = 0;
    m_NumberOfSkippedFrames = 0;
}

// Disable deblocking filter to increase performance
void Skipping_MPEG2::PermanentDisableDeblocking(bool disable)
{
    m_PermanentTurnOffDeblocking = disable ? 3 : 0;
}

// Check if deblocking should be skipped
bool Skipping_MPEG2::IsShouldSkipDeblocking(MPEG2DecoderFrame *)
{
    return (IS_SKIP_DEBLOCKING_MODE_PREVENTIVE || IS_SKIP_DEBLOCKING_MODE_PERMANENT);
}

// Check if frame should be skipped to decrease decoding delays
bool Skipping_MPEG2::IsShouldSkipFrame(MPEG2DecoderFrame * )
{
    return false;
}

// Set decoding skip frame mode
void Skipping_MPEG2::ChangeVideoDecodingSpeed(int32_t & num)
{
    m_VideoDecodingSpeed += num;

    if (m_VideoDecodingSpeed < 0)
        m_VideoDecodingSpeed = 0;
    if (m_VideoDecodingSpeed > 7)
        m_VideoDecodingSpeed = 7;

    num = m_VideoDecodingSpeed;

    int32_t deblocking_off = m_PermanentTurnOffDeblocking;
    if (deblocking_off == 3)
        m_PermanentTurnOffDeblocking = 3;
}

// Get current skip mode state
Skipping_MPEG2::SkipInfo Skipping_MPEG2::GetSkipInfo() const
{
    SkipInfo info;
    info.isDeblockingTurnedOff = (m_VideoDecodingSpeed == 5) || (m_VideoDecodingSpeed == 7);
    info.numberOfSkippedFrames = m_NumberOfSkippedFrames;
    return info;
}

/****************************************************************************************************/
// SEI_Storer_MPEG2
/****************************************************************************************************/
SEI_Storer_MPEG2::SEI_Storer_MPEG2()
{
    Reset();
}

SEI_Storer_MPEG2::~SEI_Storer_MPEG2()
{
    Close();
}

// Initialize SEI storage
void SEI_Storer_MPEG2::Init()
{
    Close();
    m_data.resize(MAX_BUFFERED_SIZE);
    m_payloads.resize(START_ELEMENTS);
    m_offset = 0;
    m_lastUsed = 2;
}

// Deallocate SEI storage
void SEI_Storer_MPEG2::Close()
{
    Reset();
    m_data.clear();
    m_payloads.clear();
}

// Reset SEI storage
void SEI_Storer_MPEG2::Reset()
{
    m_offset = 0;
    m_lastUsed = 2;
    for (uint32_t i = 0; i < m_payloads.size(); i++)
    {
        m_payloads[i].isUsed = 0;
    }
}

// Set SEI frame for stored SEI messages
void SEI_Storer_MPEG2::SetFrame(MPEG2DecoderFrame * frame)
{
    VM_ASSERT(frame);
    for (uint32_t i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == 0 && m_payloads[i].isUsed)
        {
            m_payloads[i].frame = frame;
        }
    }
}

// Set timestamp for stored SEI messages
void SEI_Storer_MPEG2::SetTimestamp(MPEG2DecoderFrame * frame)
{
    VM_ASSERT(frame);
    double ts = frame->m_dFrameTime;

    for (uint32_t i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == frame)
        {
            m_payloads[i].timestamp = ts;
            if (m_payloads[i].isUsed)
                m_payloads[i].isUsed = m_lastUsed;
        }
    }

    m_lastUsed++;
}

// Retrieve a stored SEI message which was not retrieved before
const SEI_Storer_MPEG2::SEI_Message * SEI_Storer_MPEG2::GetPayloadMessage()
{
    SEI_Storer_MPEG2::SEI_Message * msg = 0;

    for (uint32_t i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].isUsed > 1)
        {
            if (!msg || msg->isUsed > m_payloads[i].isUsed)
            {
                msg = &m_payloads[i];
            }
        }
    }

    if (msg)
        msg->isUsed = 0;

    return msg;
}

// Put a new SEI message to the storage
SEI_Storer_MPEG2::SEI_Message* SEI_Storer_MPEG2::AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type)
{
    size_t const size
        = nalUnit->GetDataSize();

    if (size > (m_data.size() >> 2))
        return 0;

    if (m_offset + size > m_data.size())
    {
        m_offset = 0;
    }

    // clear overwriting messages:
    for (uint32_t i = 0; i < m_payloads.size(); i++)
    {
        if (!m_payloads[i].isUsed)
            continue;

        SEI_Message & mmsg = m_payloads[i];

        if ((m_offset + size > mmsg.offset) &&
            (m_offset < mmsg.offset + mmsg.size))
        {
            m_payloads[i].isUsed = 0;
            return 0;
        }
    }

    size_t freeSlot = 0;
    //move empty (not used) payloads to the end of sequence
    std::vector<SEI_Message>::iterator
        end = std::remove_if(m_payloads.begin(), m_payloads.end(), std::mem_fun_ref(&SEI_Message::empty));
    if (end != m_payloads.end())
    {
        //since the state of elements after new logical end is unspecified
        //we have to clear (mark as not used) them
        std::for_each(end, m_payloads.end(), std::mem_fun_ref(&SEI_Message::clear));
        freeSlot = std::distance(m_payloads.begin(), end);
    }
    else
    {
        if (m_payloads.size() >= MAX_ELEMENTS)
            return 0;

        m_payloads.push_back(SEI_Message());
        freeSlot = m_payloads.size() - 1;
    }

    m_payloads[freeSlot].frame     = 0;
    m_payloads[freeSlot].offset    = m_offset;
    m_payloads[freeSlot].size      = size;
    m_payloads[freeSlot].data      = &m_data[m_offset];

    if (nalUnit->GetExData())
        m_payloads[freeSlot].nal_type = nalUnit->GetExData()->values[0];

    m_payloads[freeSlot].type      = type;

    m_payloads[freeSlot].isUsed = 1;

    MFX_INTERNAL_CPY(m_payloads[freeSlot].data, (uint8_t*)nalUnit->GetDataPointer(), m_payloads[freeSlot].size);

    m_offset += size;
    return &m_payloads[freeSlot];
}

ViewItem_MPEG2::ViewItem_MPEG2()
{
    Reset();

} // ViewItem_MPEG2::ViewItem_MPEG2(void)

ViewItem_MPEG2::ViewItem_MPEG2(const ViewItem_MPEG2 &src)
{
    Reset();

    pDPB.reset(src.pDPB.release());
    dpbSize = src.dpbSize;
    sps_max_dec_pic_buffering = src.sps_max_dec_pic_buffering;
    sps_max_num_reorder_pics = src.sps_max_num_reorder_pics;

} // ViewItem_MPEG2::ViewItem_MPEG2(const ViewItem_MPEG2 &src)

ViewItem_MPEG2::~ViewItem_MPEG2()
{
    Close();

} // ViewItem_MPEG2::ViewItem_MPEG2(void)

// Initialize the view, allocate resources
UMC::Status ViewItem_MPEG2::Init()
{
    // release the object before initialization
    Close();

    try
    {
        // allocate DPB and POC counter
        pDPB.reset(new MPEG2DBPList());
    }
    catch(...)
    {
        return UMC::UMC_ERR_ALLOC;
    }

    // save the ID
    localFrameTime = 0;
    pCurFrame = 0;

    return UMC::UMC_OK;

} // Status ViewItem_MPEG2::Init(uint32_t view_id)

// Close the view and release all resources
void ViewItem_MPEG2::Close(void)
{
    // Reset the parameters before close
    Reset();

    if (pDPB.get())
    {
        pDPB.reset();
    }

    dpbSize = 0;
    sps_max_dec_pic_buffering = 1;
    sps_max_num_reorder_pics = 0;

} // void ViewItem_MPEG2::Close(void)

// Reset the view and reset all resource
void ViewItem_MPEG2::Reset(void)
{
    if (pDPB.get())
    {
        pDPB->Reset();
    }

    pCurFrame = 0;
    localFrameTime = 0;
    sps_max_dec_pic_buffering = 1;
    sps_max_num_reorder_pics = 0;
    dpbSize = 0;

} // void ViewItem_MPEG2::Reset(void)

// Reset the size of DPB for particular view item
void ViewItem_MPEG2::SetDPBSize(MPEG2SeqParamSet *pSps, uint32_t & level_idc)
{
    dpbSize = 2;
    /*
    uint32_t level = level_idc ? level_idc : pSps->m_pcPTL.GetGeneralPTL()->level_idc;

    // calculate the new DPB size value

    // FIXME: should have correct temporal layer

    dpbSize = pSps->sps_max_dec_pic_buffering[pSps->sps_max_sub_layers-1];

    if (level_idc)
    {
        level_idc = level;
    }
    else
    {
        pSps->m_pcPTL.GetGeneralPTL()->level_idc = level;
    }
*/
    // provide the new value to the DPBList
    if (pDPB.get())
    {
        pDPB->SetDPBSize(dpbSize);
    }

} // void ViewItem_MPEG2::SetDPBSize(const MPEG2SeqParamSet *pSps)
/****************************************************************************************************/
// TaskSupplier_MPEG2
/****************************************************************************************************/
TaskSupplier_MPEG2::TaskSupplier_MPEG2()
    : m_SliceIdxInTaskSupplier(0)
    , m_pSegmentDecoder(0)
    , m_iThreadNum(0)
    , m_maxUIDWhenWasDisplayed(0)
    , m_local_delta_frame_time(0)
    , m_use_external_framerate(false)
    , m_decodedOrder(false)
    , m_pLastSlice(0)
    , m_pLastDisplayed(0)
    , m_pMemoryAllocator(0)
    , m_pFrameAllocator(0)
    , m_WaitForIDR(false)
    , m_prevSliceBroken(false)
    , m_RA_POC(0)
    , NoRaslOutputFlag(0)
    , m_IRAPType(NAL_UT_INVALID)
    , m_DPBSizeEx(0)
    , m_frameOrder(0)
    , m_pTaskBroker(0)
    , m_UIDFrameCounter(0)
    , m_sei_messages(0)
    , m_isInitialized(false)
{
}

TaskSupplier_MPEG2::~TaskSupplier_MPEG2()
{
    Close();
}

// Initialize task supplier and creak task broker
UMC::Status TaskSupplier_MPEG2::Init(UMC::VideoDecoderParams *init)
{
    if (NULL == init)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    m_initializationParams = *init;

    int32_t nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    AU_Splitter_MPEG2::Init(init);
    MVC_Extension::Init();

    // create slice decoder(s)
    m_pSegmentDecoder = new MPEG2SegmentDecoderBase *[m_iThreadNum];
    memset(m_pSegmentDecoder, 0, sizeof(MPEG2SegmentDecoderBase *) * m_iThreadNum);

    CreateTaskBroker();
    m_pTaskBroker->Init(m_iThreadNum);

    for (uint32_t i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC::UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC::UMC_ERR_INIT;
    }

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    m_isInitialized = true;

    return UMC::UMC_OK;
}


void TaskSupplier_MPEG2::CreateTaskBroker()
{

}

// Initialize what is necessary to decode bitstream header before the main part is initialized
UMC::Status TaskSupplier_MPEG2::PreInit(UMC::VideoDecoderParams *init)
{
    if (m_isInitialized)
        return UMC::UMC_OK;

    if (NULL == init)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    MVC_Extension::Init();

    int32_t nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    AU_Splitter_MPEG2::Init(init);

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    return UMC::UMC_OK;
}

// Release allocated resources
void TaskSupplier_MPEG2::Close()
{
    if (m_pTaskBroker)
    {
        m_pTaskBroker->Release();
    }

// from reset
    if (GetView()->pDPB.get())
    {
        for (MPEG2DecoderFrame *pFrame = GetView()->pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }

    if (m_pSegmentDecoder)
    {
        for (uint32_t i = 0; i < m_iThreadNum; i += 1)
        {
            delete m_pSegmentDecoder[i];
            m_pSegmentDecoder[i] = 0;
        }
    }

    MVC_Extension::Close();
        DecReferencePictureMarking_MPEG2::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_ObjHeap.FreeObject(m_pLastSlice);
        m_pLastSlice = 0;
    }

    AU_Splitter_MPEG2::Close();
    Skipping_MPEG2::Reset();
    m_pocDecoding.Reset();

    m_frameOrder               = 0;

    m_decodedOrder      = false;
    m_WaitForIDR        = true;
    m_prevSliceBroken   = false;
    m_maxUIDWhenWasDisplayed = 0;

    m_RA_POC = 0;
    m_IRAPType = NAL_UT_INVALID;
    NoRaslOutputFlag = 1;

    m_pLastDisplayed = 0;

    delete m_sei_messages;
    m_sei_messages = 0;

// from reset
    delete[] m_pSegmentDecoder;
    m_pSegmentDecoder = 0;

    delete m_pTaskBroker;
    m_pTaskBroker = 0;

    m_iThreadNum = 0;

    m_DPBSizeEx = 1;

    m_isInitialized = false;

} // void TaskSupplier_MPEG2::Close()

// Reset to default state
void TaskSupplier_MPEG2::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    {
        for (MPEG2DecoderFrame *pFrame = GetView()->pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }

    if (m_sei_messages)
        m_sei_messages->Reset();

    MVC_Extension::Reset();

    DecReferencePictureMarking_MPEG2::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_ObjHeap.FreeObject(m_pLastSlice);
        m_pLastSlice = 0;
    }

    Skipping_MPEG2::Reset();
    AU_Splitter_MPEG2::Reset();
    m_pocDecoding.Reset();

    m_frameOrder               = 0;

    m_decodedOrder      = false;
    m_WaitForIDR        = true;
    m_prevSliceBroken   = false;
    m_maxUIDWhenWasDisplayed = 0;

    m_RA_POC = 0;
    m_IRAPType = NAL_UT_INVALID;
    NoRaslOutputFlag = 1;

    m_pLastDisplayed = 0;

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum);
}

// Attempt to recover after something unexpectedly went wrong
void TaskSupplier_MPEG2::AfterErrorRestore()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    GetView()->pDPB->Reset();

    MVC_Extension::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_ObjHeap.FreeObject(m_pLastSlice);
        m_pLastSlice = 0;
    }

    Skipping_MPEG2::Reset();
    AU_Splitter_MPEG2::Reset();

    m_decodedOrder      = false;
    m_WaitForIDR        = true;
    m_prevSliceBroken   = false;
    m_maxUIDWhenWasDisplayed = 0;
    NoRaslOutputFlag = 1;

    m_pLastDisplayed = 0;

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum);
}

// Fill up current bitstream information
UMC::Status TaskSupplier_MPEG2::GetInfo(UMC::VideoDecoderParams *lpInfo)
{
    MPEG2SequenceHeader *seq = m_Headers.m_SequenceParam.GetHeader();
    if (!seq)
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    MPEG2SequenceExtension *seqExt = seq->GetSeqExt();
    if (!seqExt)
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    lpInfo->info.stream_type = UMC::MPEG2_VIDEO;

    lpInfo->info.clip_info.height = seq->horizontal_size_value;
    lpInfo->info.clip_info.width = seq->vertical_size_value;

    mfxU32 frameRateExtN, frameRateExtD;
    GetMfxFrameRate(seqExt->frame_rate_code, frameRateExtN, frameRateExtD);

    lpInfo->info.framerate = frameRateExtN/frameRateExtD;

    // Table 8-1 – Meaning of bits in profile_and_level_indication
    lpInfo->profile = GetMfxCodecProfile((seqExt->profile_and_level_indication >> 4) & 7);
    lpInfo->level = GetMfxCodecLevel(seqExt->profile_and_level_indication & 0xF);

    lpInfo->numThreads = m_iThreadNum;

    lpInfo->info.color_format = GetUMCColorFormat_MPEG2(seqExt->chroma_format);

    uint16_t aspectRatioW, aspectRatioH;
    CalcAspectRatio(seq->aspect_ratio_information, seq->horizontal_size_value, seq->vertical_size_value,
                    aspectRatioW, aspectRatioH);

    lpInfo->info.aspect_ratio_width  = aspectRatioW;
    lpInfo->info.aspect_ratio_height = aspectRatioH;

    // bit_rate is a 30-bit integer. The lower 18 bits of the integer are in bit_rate_value and the upper 12 bits are in bit_rate_extension
    uint64_t bitrate;
    if (seqExt->bit_rate_extension)
    {
        bitrate = seq->bit_rate_value | (seqExt->bit_rate_extension << 18);

        if (bitrate >= 0x40000000/100) // check if fit to 32u
            bitrate = 0xffffffff;
        else
            bitrate *= 400;
    }
    else
        bitrate = seq->bit_rate_value * 400;

    lpInfo->info.bitrate = bitrate;

    // UMC::INTERLEAVED_TOP_FIELD_FIRST actually should be UNKNOWN but we don't have this type
    lpInfo->info.interlace_type = seqExt->progressive_sequence  ? UMC::PROGRESSIVE : UMC::INTERLEAVED_TOP_FIELD_FIRST;

    return UMC::UMC_OK;
}

// Search DPB for a frame which may be reused
MPEG2DecoderFrame *TaskSupplier_MPEG2::GetFreeFrame()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    MPEG2DecoderFrame *pFrame = 0;

    ViewItem_MPEG2 *pView = GetView();
    MPEG2DBPList *pDPB = pView->pDPB.get();

    // Traverse list for next disposable frame
    if (pDPB->countAllFrames() >= pView->dpbSize + m_DPBSizeEx)
        pFrame = pDPB->GetOldestDisposable();

    //pDPB->printDPB();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= pView->dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new MPEG2DecoderFrame(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        pDPB->append(pFrame);
    }

    pFrame->Reset();

    pFrame->m_decOrder     = m_UIDFrameCounter;
    pFrame->m_displayOrder = pFrame->m_decOrder;

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->IncrementReference();

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;

    return pFrame;
}

// Decode SEI NAL unit
UMC::Status TaskSupplier_MPEG2::DecodeSEI(UMC::MediaDataEx *nalUnit)
{
    if (m_Headers.m_SeqParams.GetCurrentID() == -1)
        return UMC::UMC_OK;

    MPEG2HeadersBitstream bitStream;

    try
    {
        MemoryPiece mem;
        mem.SetData(nalUnit);

        MemoryPiece swappedMem;
        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem, 0);

        bitStream.Reset((uint8_t*)swappedMem.GetPointer(), (uint32_t)swappedMem.GetDataSize());

        NalUnitType nal_unit_type;
        uint32_t temporal_id;

        bitStream.GetNALUnitType(nal_unit_type, temporal_id);

        do
        {
            MPEG2SEIPayLoad    m_SEIPayLoads;

            /*int32_t target_sps =*/ bitStream.ParseSEI(m_Headers.m_SeqParams,
                m_Headers.m_SeqParams.GetCurrentID(), &m_SEIPayLoads);

            if (m_SEIPayLoads.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
            {
                m_UserData = m_SEIPayLoads;
            }
            else
            {
                m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
            }

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC::UMC_OK;
}


UMC::Status TaskSupplier_MPEG2::xDecodeSequenceHeader(MPEG2HeadersBitstream *bs)
{
    MPEG2SequenceHeader seq;

    bs->GetSequenceHeader(&seq);

    m_Headers.m_SequenceParam.AddHeader(&seq);

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_MPEG2::xDecodeSequenceExt(MPEG2HeadersBitstream *bs)
{
    MPEG2SequenceExtension seqExt;

    bs->GetSequenceExtension(&seqExt);
    MPEG2SequenceHeader * seq = m_Headers.m_SequenceParam.GetHeader();
    if (seq)
    {
        seq->SetSeqExt(seqExt);
    }

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_MPEG2::xDecodeSequenceDisplayExt(MPEG2HeadersBitstream *bs)
{
    MPEG2SequenceDisplayExtension dispExt;

    bs->GetSequenceDisplayExtension(&dispExt);
    MPEG2SequenceHeader * seq = m_Headers.m_SequenceParam.GetHeader();
    if (seq)
    {
        seq->SetSeqDisplay(dispExt);
    }

    return UMC::UMC_OK;
}


UMC::Status TaskSupplier_MPEG2::xDecodePictureHeader(MPEG2HeadersBitstream *bs)
{
    MPEG2PictureHeader pic;

    bs->GetPictureHeader(&pic);

    m_Headers.m_PictureHeader.AddHeader(&pic);

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_MPEG2::xDecodePictureHeaderExt(MPEG2HeadersBitstream *bs)
{
    MPEG2PictureCodingExtension picExt;

    bs->GetPictureExtensionHeader(&picExt);

    MPEG2PictureHeader * pic = m_Headers.m_PictureHeader.GetHeader();
    if (pic)
    {
        pic->SetPicExt(picExt);
    }

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_MPEG2::xDecodeQuantMatrix(MPEG2HeadersBitstream *bs)
{
    MPEG2QuantMatrix quantMatrix;

    bs->GetQuantMatrix(&quantMatrix);

    MPEG2PictureHeader * pic = m_Headers.m_PictureHeader.GetHeader();
    if (pic)
    {
        pic->SetQuantMatrix(quantMatrix);
    }

    return UMC::UMC_OK;
}

// Decode a bitstream header NAL unit
UMC::Status TaskSupplier_MPEG2::DecodeHeaders(UMC::MediaDataEx *nalUnit)
{
    UMC::Status umcRes = UMC::UMC_OK;

    MPEG2HeadersBitstream bitStream;

    try
    {
        MemoryPiece mem;
        mem.SetData(nalUnit);

        MemoryPiece swappedMem;

        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem, 0);

        bitStream.Reset((uint8_t*)swappedMem.GetPointer(), (uint32_t)swappedMem.GetDataSize());

        NalUnitType nal_unit_type;

        bitStream.GetNALUnitType(nal_unit_type);

        switch(nal_unit_type)
        {
        case NAL_UT_SEQUENCE_HEADER:
            umcRes = xDecodeSequenceHeader(&bitStream);
            break;
        case NAL_UT_PICTURE_HEADER:
            umcRes = xDecodePictureHeader(&bitStream);
            break;
        case NAL_UT_EXTENSION:
        {
            NalUnitTypeExt nal_unit_type_ext = (NalUnitTypeExt)bitStream.GetBits(4);
            switch(nal_unit_type_ext)
            {
            case NAL_UT_EXT_SEQUENCE_EXTENSION:
                umcRes = xDecodeSequenceExt(&bitStream);
                break;
            case NAL_UT_EXT_SEQUENCE_DISPLAY_EXTENSION:
                umcRes = xDecodeSequenceDisplayExt(&bitStream);
                break;
            case NAL_UT_EXT_PICTURE_CODING_EXTENSION:
                umcRes = xDecodePictureHeaderExt(&bitStream);
                break;
            case NAL_UT_EXT_QUANT_MATRIX_EXTENSION:
                umcRes = xDecodePictureHeaderExt(&bitStream);
                break;
            default:
                break;
            }
        }
            break;
        default:
            break;
        }
    }
    catch(const mpeg2_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC::UMC_ERR_INVALID_STREAM;
    }

    return umcRes;
}

// Set frame display time
void TaskSupplier_MPEG2::PostProcessDisplayFrame(MPEG2DecoderFrame *pFrame)
{
    if (!pFrame || pFrame->post_procces_complete)
        return;

    ViewItem_MPEG2 &view = *GetView();

    pFrame->m_isOriginalPTS = pFrame->m_dFrameTime > -1.0;
    if (pFrame->m_isOriginalPTS)
    {
        view.localFrameTime = pFrame->m_dFrameTime;
    }
    else
    {
        pFrame->m_dFrameTime = view.localFrameTime;
    }

    pFrame->m_frameOrder = m_frameOrder;
    switch (pFrame->m_DisplayPictureStruct_MPEG2)
    {
    case DPS_TOP_BOTTOM_TOP_MPEG2:
    case DPS_BOTTOM_TOP_BOTTOM_MPEG2:
        if (m_initializationParams.lFlags & UMC::FLAG_VDEC_TELECINE_PTS)
        {
            view.localFrameTime += (m_local_delta_frame_time / 2);
        }
        break;
    default:
        break;
    }

    view.localFrameTime += m_local_delta_frame_time;

    m_frameOrder++;

    pFrame->post_procces_complete = true;

    DEBUG_PRINT((VM_STRING("Outputted %s, pppp - %d\n"), GetFrameInfoString(pFrame), pppp++));

    m_maxUIDWhenWasDisplayed = MFX_MAX(m_maxUIDWhenWasDisplayed, pFrame->m_maxUIDWhenWasDisplayed);
}

// Find a next frame ready to be output from decoder
MPEG2DecoderFrame *TaskSupplier_MPEG2::GetFrameToDisplayInternal(bool force)
{
    ViewItem_MPEG2 &view = *GetView();

    for (;;)
    {
        uint32_t countDisplayable = 0;
        int32_t maxUID = 0;
        uint32_t countDPBFullness = 0;

        view.pDPB->calculateInfoForDisplay(countDisplayable, countDPBFullness, maxUID);

        // show oldest frame
        if (countDisplayable || force)
        {
            MPEG2DecoderFrame *pTmp = view.pDPB->findOldestDisplayable(view.dpbSize);

            if (pTmp)
            {
                return pTmp;
            }
        }

        break;
    }

    return 0;
}

// If a frame has all slices found, add it to asynchronous decode queue
UMC::Status TaskSupplier_MPEG2::CompleteDecodedFrames(MPEG2DecoderFrame ** decoded)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskSupplier_MPEG2::CompleteDecodedFrames");
    MPEG2DecoderFrame* completed = 0;
    UMC::Status sts = UMC::UMC_OK;

    ViewItem_MPEG2 &view = *GetView();
    for (;;) //add all ready to decoding
    {
        bool isOneToAdd = true;
        MPEG2DecoderFrame * frameToAdd = 0;

        for (MPEG2DecoderFrame * frame = view.pDPB->head(); frame; frame = frame->future())
        {
            int32_t const frm_error = frame->GetError();

            //we don't overwrite an error if we already got it
            if (sts == UMC::UMC_OK && frm_error < 0)
                //if we have ERROR_FRAME_DEVICE_FAILURE  bit is set then this error is  UMC::Status code
                sts = static_cast<UMC::Status>(frm_error);

            if (!frame->IsDecoded())
            {
                if (!frame->IsDecodingStarted() && frame->IsFullFrame())
                {
                    if (frameToAdd)
                    {
                        isOneToAdd = false;
                        if (frameToAdd->m_UID < frame->m_UID) // add first with min UID
                            continue;
                    }

                    frameToAdd = frame;
                }

                if (!frame->IsDecodingCompleted())
                {
                    continue;
                }

                DEBUG_PRINT((VM_STRING("Decode %s \n"), GetFrameInfoString(frame)));
                frame->OnDecodingCompleted();
                completed = frame;;
            }
        }

        if (sts != UMC::UMC_OK)
            break;

        if (frameToAdd)
        {
            if (!m_pTaskBroker->AddFrameToDecoding(frameToAdd))
                break;
        }

        if (isOneToAdd)
            break;
    }

    if (decoded)
        *decoded = completed;

    return sts;
}

// Add a new bitstream data buffer to decoding
UMC::Status TaskSupplier_MPEG2::AddSource(UMC::MediaData * pSource)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskSupplier_MPEG2::AddSource");
    MPEG2DecoderFrame* completed = 0;
    UMC::Status umcRes = CompleteDecodedFrames(&completed);
    if (umcRes != UMC::UMC_OK)
        return pSource || !completed ? umcRes : UMC::UMC_OK;

    if (GetFrameToDisplayInternal(false))
        return UMC::UMC_OK;

    umcRes = AddOneFrame(pSource); // construct frame

    if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
    {
        ViewItem_MPEG2 &view = *GetView();

        int32_t count = 0;
        for (MPEG2DecoderFrame *pFrame = view.pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            count++;
            // frame is being processed. Wait for asynchronous end of operation.
            if (pFrame->isDisposable() || isInDisplayngStage(pFrame) || isAlmostDisposable(pFrame))
            {
                return UMC::UMC_WRN_INFO_NOT_READY;
            }
        }

        if (count < view.pDPB->GetDPBSize())
            return UMC::UMC_WRN_INFO_NOT_READY;

        // some more hard reasons of frame lacking.
        if (!m_pTaskBroker->IsEnoughForStartDecoding(true))
        {
            umcRes = CompleteDecodedFrames(&completed);
            if (umcRes != UMC::UMC_OK)
                return umcRes;
            else if (completed)
                return UMC::UMC_WRN_INFO_NOT_READY;

            if (GetFrameToDisplayInternal(true))
                return UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            return UMC::UMC_WRN_INFO_NOT_READY;
        }
    }

    return umcRes;
}

// Choose appropriate processing action for specified NAL unit
UMC::Status TaskSupplier_MPEG2::ProcessNalUnit(UMC::MediaDataEx *nalUnit)
{
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    NalUnitType unitType = (NalUnitType)pMediaDataEx->values[pMediaDataEx->index];

    switch(unitType)
    {
    case NAL_UT_SEQUENCE_HEADER:
    case NAL_UT_PICTURE_HEADER:
    case NAL_UT_EXTENSION:
        umcRes = DecodeHeaders(nalUnit);
        break;
/*
    case NAL_UT_CODED_SLICE_TRAIL_R:
    case NAL_UT_CODED_SLICE_TRAIL_N:
    case NAL_UT_CODED_SLICE_TLA_R:
    case NAL_UT_CODED_SLICE_TSA_N:
    case NAL_UT_CODED_SLICE_STSA_R:
    case NAL_UT_CODED_SLICE_STSA_N:
    case NAL_UT_CODED_SLICE_BLA_W_LP:
    case NAL_UT_CODED_SLICE_BLA_W_RADL:
    case NAL_UT_CODED_SLICE_BLA_N_LP:
    case NAL_UT_CODED_SLICE_IDR_W_RADL:
    case NAL_UT_CODED_SLICE_IDR_N_LP:
    case NAL_UT_CODED_SLICE_CRA:
    case NAL_UT_CODED_SLICE_RADL_R:
    case NAL_UT_CODED_SLICE_RASL_R:
        if (MPEG2Slice * pSlice = DecodeSliceHeader(nalUnit))
            umcRes = AddSlice(pSlice, false);
        break;

    case NAL_UT_VPS:
    case NAL_UT_SPS:
    case NAL_UT_PPS:
        umcRes = DecodeHeaders(nalUnit);
        break;

    case NAL_UT_SEI:
        umcRes = DecodeSEI(nalUnit);
        break;

    case NAL_UT_AU_DELIMITER:
        umcRes = AddSlice(0, false);
        break;
*/
    default:
        break;
    };
    if (unitType >= 0x1 && unitType <= 0xAF)
    {
        if (MPEG2Slice * pSlice = DecodeSliceHeader(nalUnit))
            umcRes = AddSlice(pSlice, false);
    }

    return umcRes;
}

// Find NAL units in new bitstream buffer and process them
UMC::Status TaskSupplier_MPEG2::AddOneFrame(UMC::MediaData * pSource)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskSupplier_MPEG2::AddOneFrame");
    if (m_pLastSlice)
    {
        UMC::Status sts = AddSlice(m_pLastSlice, !pSource);
        if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
            return sts;
    }

    size_t moveToSpsOffset =  0;

    do
    {
        UMC::MediaDataEx *nalUnit = m_pNALSplitter->GetNalUnits(pSource);
        if (!nalUnit)
            break;

        UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

        for (int32_t i = 0; i < (int32_t)pMediaDataEx->count; i++, pMediaDataEx->index ++)
        {
            NalUnitType nut = static_cast<NalUnitType>(pMediaDataEx->values[i]);

            switch (nut)
            {
            case NAL_UT_PICTURE_HEADER:
            {
                UMC::Status umsRes = CompleteCurrentFrameOrField(false);
                if (umsRes == UMC::UMC_OK)
                {
                    uint32_t nalIndex = pMediaDataEx->index;
                    uint32_t size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];
                    pSource->MoveDataPointer(- size - 4); // since we're going to exit, need to restore position to the header to handle it later

                    return UMC::UMC_OK;
                }
            }
            case NAL_UT_SEQUENCE_HEADER:
            case NAL_UT_EXTENSION:
                {
                    UMC::Status umsRes = DecodeHeaders(nalUnit);
                    if (umsRes != UMC::UMC_OK)
                    {
                        if (umsRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                            (nut == NAL_UT_SPS && umsRes == UMC::UMC_ERR_INVALID_STREAM))
                        {

                            int32_t nalIndex = pMediaDataEx->index;
                            int32_t size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

                            if (AddSlice(0, !pSource) == UMC::UMC_OK)
                            {
                                pSource->MoveDataPointer(- size - 3);
                                return UMC::UMC_OK;
                            }
                            moveToSpsOffset = pSource->GetDataSize() + size + 3;
                            continue;
                        }

                        return umsRes;
                    }
                }
                break;
            default:
                break;
            };
            if (nut >= 0x1 && nut <= 0xAF)
            {
                if(MPEG2Slice *pSlice = DecodeSliceHeader(nalUnit))
                {
                    UMC::Status sts = AddSlice(pSlice, !pSource);
                    if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                        return sts;
                }
            }
        }

    } while ((pSource) && (MINIMAL_DATA_SIZE_MPEG2 < pSource->GetDataSize()));

    if (!pSource)
    {
        return CompleteCurrentFrameOrField(true);
    }
    else
    {
        uint32_t flags = pSource->GetFlags();

        if (!(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            VM_ASSERT(!m_pLastSlice);
            return CompleteCurrentFrameOrField(true);
        }
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

// Decode slice header start, set slice links to SPS and PPS and correct tile offsets table if needed
MPEG2Slice *TaskSupplier_MPEG2::DecodeSliceHeader(UMC::MediaDataEx *nalUnit)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskSupplier_MPEG2::DecodeSliceHeader");

    if (!m_Headers.m_SequenceParam.GetHeader() || !m_Headers.m_SequenceParam.GetHeader()->GetSeqExt() ||
        !m_Headers.m_PictureHeader.GetHeader() || !m_Headers.m_PictureHeader.GetHeader()->GetPicExt())
    {
        return 0;
    }


    MPEG2Slice * pSlice = m_ObjHeap.AllocateObject<MPEG2Slice>();
    pSlice->IncrementReference();

    notifier0<MPEG2Slice> memory_leak_preventing_slice(pSlice, &MPEG2Slice::DecrementReference);

    MemoryPiece memCopy;
    memCopy.SetData(nalUnit);

    pSlice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

    notifier0<MemoryPiece> memory_leak_preventing(&pSlice->m_source, &MemoryPiece::Release);

    std::vector<uint32_t> removed_offsets(0);
    SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    swapper->SwapMemory(&pSlice->m_source, &memCopy, &removed_offsets);

    pSlice->SetSeqHeader(m_Headers.m_SequenceParam.GetHeader());
    if (!pSlice->GetSeqHeader())
    {
        return 0;
    }

    if (!pSlice->GetSeqHeader()->GetSeqExt())
    {
        return 0;
    }

    pSlice->SetPicHeader(m_Headers.m_PictureHeader.GetHeader());
    if (!pSlice->GetPicHeader())
    {
        return 0;
    }

    if (!pSlice->GetPicHeader()->GetPicExt())
    {
        return 0;
    }

    pSlice->m_pCurrentFrame = NULL;

    memory_leak_preventing.ClearNotification();

    bool ready = pSlice->Reset(&m_pocDecoding);
    if (!ready)
    {
        m_prevSliceBroken = pSlice->IsError();
        return 0;
    }

    MPEG2SliceHeader_ * sliceHdr = pSlice->GetSliceHeader_();
    VM_ASSERT(sliceHdr);

    uint32_t currOffset = sliceHdr->m_HeaderBitstreamOffset;
    uint32_t currOffsetWithEmul = currOffset;


    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    //for SliceIdx m_SliceIdx m_iNumber
    pSlice->m_iNumber = m_SliceIdxInTaskSupplier;
    m_SliceIdxInTaskSupplier++;
    return pSlice;
}

UMC::Status TaskSupplier_MPEG2::CompleteCurrentFrameOrField(bool force)
{
    if (m_pLastSlice)
        throw mpeg2_exception(UMC::UMC_ERR_FAILED); // unexpected behavior

    if (NULL == GetView()->pCurFrame)
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    CompleteFrame(GetView()->pCurFrame);

    OnFullFrame(GetView()->pCurFrame);
    GetView()->pCurFrame = NULL;

    if (force)
    {
        ReorderFrames(0);
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC::UMC_OK;
}

// Add a new slice to frame
UMC::Status TaskSupplier_MPEG2::AddSlice(MPEG2Slice * pSlice, bool force)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskSupplier_MPEG2::AddSlice");
    m_pLastSlice = 0;

    ViewItem_MPEG2 &view = *GetView();
    MPEG2DecoderFrame * pFrame = view.pCurFrame;

    // there is no free frames.
    // try to allocate a new frame.
    if (!pFrame)
    {
        // allocate a new frame, initialize it with slice's parameters.
        pFrame = AllocateNewFrame(pSlice);
        if (!pFrame)
        {
            view.pCurFrame = NULL;
            m_pLastSlice = pSlice;
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
        }

        // set the current being processed frame
        view.pCurFrame = pFrame;

        SetReferences(pFrame);
    }

    // add the next slice to the initialized frame.
    pSlice->m_pCurrentFrame = pFrame;
    AddSliceToFrame(pFrame, pSlice);

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

void TaskSupplier_MPEG2::SetReferences(MPEG2DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    if (MPEG2_P_PICTURE == (FrameType)pFrame->m_FrameType)
    {
        MPEG2DecoderFrame * pClosestRef = nullptr;
        uint32_t distance = 0xffffffff;
        for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp == pFrame || !pTmp->isShortTermRef())
                continue;

            if ((pFrame->m_decOrder - pTmp->m_decOrder) < distance)
            {
                distance = pFrame->m_decOrder - pTmp->m_decOrder;
                pClosestRef = pTmp;
            }
        }
        pFrame->SetForwardRefPic(pClosestRef);
    }

    if (MPEG2_B_PICTURE == (FrameType)pFrame->m_FrameType)
    {
        MPEG2DecoderFrame * pClosestRef = nullptr;
        uint32_t distance = 0xffffffff;
        for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp == pFrame || !pTmp->isShortTermRef())
                continue;

            if ((pFrame->m_decOrder - pTmp->m_decOrder) < distance)
            {
                distance = pFrame->m_decOrder - pTmp->m_decOrder;
                pClosestRef = pTmp;
            }
        }
        pFrame->SetBackwardRefPic(pClosestRef);

        MPEG2DecoderFrame * pAnotherClosestRef = nullptr;
        distance = 0xffffffff;
        for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp == pFrame || !pTmp->isShortTermRef())
                continue;

            if (((pFrame->m_decOrder - pTmp->m_decOrder) < distance) && (pTmp->m_decOrder < pClosestRef->m_decOrder))
            {
                distance = pFrame->m_decOrder - pTmp->m_decOrder;
                pAnotherClosestRef = pTmp;
            }
        }
        pFrame->SetForwardRefPic(pAnotherClosestRef);
    }
}

// Mark frame as full with slices
void TaskSupplier_MPEG2::OnFullFrame(MPEG2DecoderFrame * pFrame)
{
    pFrame->SetFullFrame(true);

    if (!pFrame->GetAU()->GetSlice(0)) // seems that it was skipped and slices was dropped
        return;

    if (pFrame->GetAU()->GetSlice(0)->GetSliceHeader()->IdrPicFlag && !(pFrame->GetError() & UMC::ERROR_FRAME_DPB))
    {
        DecReferencePictureMarking_MPEG2::ResetError();
    }

    if (DecReferencePictureMarking_MPEG2::GetDPBError())
    {
        pFrame->SetErrorFlagged(UMC::ERROR_FRAME_DPB);
    }
}

// Check whether all slices for the frame were found
void TaskSupplier_MPEG2::CompleteFrame(MPEG2DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    MPEG2DecoderFrameInfo * slicesInfo = pFrame->GetAU();

    if (slicesInfo->GetStatus() > MPEG2DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d) type - %d, count - %d, m_uid - %d, IDR - %d\n"), pFrame->m_PicOrderCnt, pFrame->m_FrameType, slicesInfo->GetSliceCount(), pFrame->m_UID, slicesInfo->GetAnySlice()->GetSliceHeader()->IdrPicFlag));

    m_prevSliceBroken = false;

    // skipping algorithm
    const MPEG2Slice *slice = slicesInfo->GetSlice(0);
    if (!slice || IsShouldSkipFrame(pFrame))
    {
        slicesInfo->SetStatus(MPEG2DecoderFrameInfo::STATUS_COMPLETED);

        pFrame->SetisShortTermRef(false);
        pFrame->SetisLongTermRef(false);
        pFrame->OnDecodingCompleted();
        pFrame->SetisDisplayable(false);
        pFrame->m_pic_output = false;
        return;
    }

    slicesInfo->SetStatus(MPEG2DecoderFrameInfo::STATUS_FILLED);
}

// Initialize just allocated frame with slice parameters
UMC::Status TaskSupplier_MPEG2::InitFreeFrame(MPEG2DecoderFrame * pFrame, const MPEG2Slice *pSlice)
{
    UMC::Status umcRes = UMC::UMC_OK;
    const MPEG2PictureHeader *     pic    = pSlice->GetPicHeader();
    const MPEG2SequenceHeader *    seq    = pSlice->GetSeqHeader();
    const MPEG2SequenceExtension * seqExt = seq->GetSeqExt();

    pFrame->m_FrameType = (UMC::FrameType)pic->picture_coding_type;
    pFrame->m_dFrameTime = pSlice->m_source.GetTime();
    pFrame->m_crop_left = 0;
    pFrame->m_crop_right = seq->horizontal_size_value;
    pFrame->m_crop_top = seq->vertical_size_value;
    pFrame->m_crop_bottom = 0;
    pFrame->m_crop_flag = false;

    uint16_t aspectRatioW, aspectRatioH;
    CalcAspectRatio(seq->aspect_ratio_information, seq->horizontal_size_value, seq->vertical_size_value,
                    aspectRatioW, aspectRatioH);
    pFrame->m_aspect_width  = aspectRatioW;
    pFrame->m_aspect_height = aspectRatioH;

    int32_t bit_depth = 8;

    mfxSize dimensions = {static_cast<int>(seq->horizontal_size_value), static_cast<int>(seq->vertical_size_value)};

    UMC::ColorFormat cf = GetUMCColorFormat_MPEG2(seqExt->chroma_format);

    if (cf == UMC::YUV420) //  msdk !!!
        cf = UMC::NV12;

    UMC::VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, cf, bit_depth);

    pFrame->Init(&info);

    return umcRes;
}

// Allocate frame internals
UMC::Status TaskSupplier_MPEG2::AllocateFrameData(MPEG2DecoderFrame * pFrame, mfxSize dimensions, const MPEG2SeqParamSet* pSeqParamSet, const MPEG2PicParamSet *pPicParamSet)
{
    UMC::ColorFormat color_format = pFrame->GetColorFormat();
        //(ColorFormat) pSeqParamSet->chroma_format_idc;
    UMC::VideoDataInfo info;
    int32_t bit_depth = pSeqParamSet->need16bitOutput ? 10 : 8;
    info.Init(dimensions.width, dimensions.height, color_format, bit_depth);

    UMC::FrameMemID frmMID;
    UMC::Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC::UMC_OK)
    {
        throw mpeg2_exception(UMC::UMC_ERR_ALLOC);
    }

    const UMC::FrameData *frmData = m_pFrameAllocator->Lock(frmMID);

    if (!frmData)
        throw mpeg2_exception(UMC::UMC_ERR_LOCK);

    pFrame->allocate(frmData, &info);
    pFrame->m_index = frmMID;

    (void)pPicParamSet;

    return UMC::UMC_OK;
}

// Try to find a reusable frame or allocate a new one and initialize it with slice parameters
MPEG2DecoderFrame * TaskSupplier_MPEG2::AllocateNewFrame(const MPEG2Slice *pSlice)
{
    MPEG2DecoderFrame *pFrame = NULL;

    if (!pSlice)
    {
        return NULL;
    }

    ViewItem_MPEG2 &view = *GetView();
    view.SetDPBSize(const_cast<MPEG2SeqParamSet*>(pSlice->GetSeqParam()), m_level_idc);
/*
    view.sps_max_dec_pic_buffering = pSlice->GetSeqParam()->sps_max_dec_pic_buffering[pSlice->GetSliceHeader()->nuh_temporal_id] ?
                                    pSlice->GetSeqParam()->sps_max_dec_pic_buffering[pSlice->GetSliceHeader()->nuh_temporal_id] :
                                    view.dpbSize;

    view.sps_max_num_reorder_pics = MFX_MIN(pSlice->GetSeqParam()->sps_max_num_reorder_pics[HighestTid], view.sps_max_dec_pic_buffering);
*/
    DPBUpdate(pSlice);

    pFrame = GetFreeFrame();
    if (NULL == pFrame)
    {
        return NULL;
    }

    UMC::Status umcRes = InitFreeFrame(pFrame, pSlice);
    if (umcRes != UMC::UMC_OK)
    {
        return 0;
    }

    pFrame->SetisShortTermRef(MPEG2_B_PICTURE != (FrameType)pFrame->m_FrameType);

    umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pSlice->GetSeqParam(), pSlice->GetPicParam());
    if (umcRes != UMC::UMC_OK)
    {
        return 0;
    }

    if (m_UserData.user_data.size())
    {
        pFrame->m_UserData = m_UserData;
        m_UserData.user_data.clear();
    }

    if (m_sei_messages)
    {
        m_sei_messages->SetFrame(pFrame);
    }
/*
    MPEG2SEIPayLoad * payload = m_Headers.m_SEIParams.GetHeader(SEI_PIC_TIMING_TYPE);
    if (payload && pSlice->GetSeqParam()->frame_field_info_present_flag)
    {
        pFrame->m_DisplayPictureStruct_MPEG2 = payload->SEI_messages.pic_timing.pic_struct;
    }
    else
*/
    {
        pFrame->m_DisplayPictureStruct_MPEG2 = DPS_FRAME_MPEG2;
    }

    InitFrameCounter(pFrame, pSlice);

    return pFrame;

} // MPEG2DecoderFrame * TaskSupplier_MPEG2::AllocateNewFrame(const MPEG2Slice *pSlice)

void TaskSupplier_MPEG2::InitFrameCounter(MPEG2DecoderFrame * pFrame, const MPEG2Slice *pSlice)
{
    const MPEG2PictureHeader * pic = pSlice->GetPicHeader();
    pFrame->setPicOrderCnt(pic->temporal_reference);

    DEBUG_PRINT((VM_STRING("Init frame %s\n"), GetFrameInfoString(pFrame)));

    ReorderFrames(pFrame);
}

void TaskSupplier_MPEG2::ReorderFrames(MPEG2DecoderFrame * pFrame)
{
    if (pFrame)
    {
        // - If a current frame is a B-frame, then output it immediately.
        // - if a current frame is a I-frame or P-frame, then output previous (in decoded order) I-frame or P-frame.
        if (MPEG2_B_PICTURE == (FrameType)pFrame->m_FrameType)
        {
            pFrame->SetisDisplayable(true);

            uint32_t distance = 0xffffffff;
            MPEG2DecoderFrame * pLatestIPFrame = nullptr;

            for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
            {
                if ((pTmp == pFrame) || (MPEG2_B_PICTURE == (FrameType)pTmp->m_FrameType))
                    continue;

                if (std::abs(pFrame->m_decOrder - pTmp->m_decOrder) < distance)
                {
                    distance = std::abs(pFrame->m_decOrder - pTmp->m_decOrder);
                    pLatestIPFrame = pTmp;
                }
            }

            if (pLatestIPFrame)
            {
                // Here we build display order in case of reordered frames:
                pFrame->m_displayOrder         = pLatestIPFrame->m_displayOrder; // 1. Output B frames sooner than their decoded order
                pLatestIPFrame->m_displayOrder = pFrame->m_decOrder;             // 2. Delay displaying I/P frames accordingly
            }
        }
        else
        {
            uint32_t distance = 0xffffffff;
            MPEG2DecoderFrame * pLatestIPFrame = nullptr;

            for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
            {
                if (pTmp == pFrame || MPEG2_B_PICTURE == (FrameType)pTmp->m_FrameType)
                    continue;

                // Find previous I or P in decoded order ...
                if (std::abs(pFrame->m_decOrder - pTmp->m_decOrder) < distance)
                {
                    distance = std::abs(pFrame->m_decOrder - pTmp->m_decOrder);
                    pLatestIPFrame = pTmp;
                }
            }
            // ... and make it displayable
            if (pLatestIPFrame)
            {
                pLatestIPFrame->SetisDisplayable(true);
            }
        }
    }
    else
    {
        for (MPEG2DecoderFrame * pTmp = GetView()->pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp->IsFullFrame())
            {
                pTmp->SetisDisplayable(true);
            }
        }
        return;
    }
}

// Include a new slice into a set of frame slices
void TaskSupplier_MPEG2::AddSliceToFrame(MPEG2DecoderFrame *pFrame, MPEG2Slice *pSlice)
{
    pFrame->AddSlice(pSlice);
}

// Update DPB contents marking frames for reuse
void TaskSupplier_MPEG2::DPBUpdate(const MPEG2Slice * slice)
{
    const MPEG2PictureHeader*  pic = slice->GetPicHeader();
    if (MPEG2_B_PICTURE == pic->picture_coding_type)
        return;

    uint32_t NumShortTermRefs, NumLongTermRefs;

    // find out how many active reference frames currently in decoded
    // frames buffer
    ViewItem_MPEG2 &view = *GetView();
    view.pDPB->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
    while (NumShortTermRefs > 0 &&
        (NumShortTermRefs + NumLongTermRefs >= 2))
    {
        // mark oldest short-term reference as unused
        VM_ASSERT(NumShortTermRefs > 0);

        MPEG2DecoderFrame * pFrame = view.pDPB->findOldestShortTermRef();

        if (!pFrame)
            break;

        pFrame->SetisShortTermRef(false);
        NumShortTermRefs--;
    }
}

// Find a decoder frame instance with specified surface ID
MPEG2DecoderFrame * TaskSupplier_MPEG2::FindSurface(UMC::FrameMemID id)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MPEG2DecoderFrame *pFrame = GetView()->pDPB->head();
    for (; pFrame; pFrame = pFrame->future())
    {
        if (pFrame->GetFrameData()->GetFrameMID() == id)
            return pFrame;
    }

    return 0;
}

// Start asynchronous decoding
UMC::Status TaskSupplier_MPEG2::RunDecoding()
{
    UMC::Status umcRes = CompleteDecodedFrames(0);
    if (umcRes != UMC::UMC_OK)
        return umcRes;

    MPEG2DecoderFrame *pFrame = GetView()->pDPB->head();

    for (; pFrame; pFrame = pFrame->future())
    {
        if (!pFrame->IsDecodingCompleted())
        {
            break;
        }
    }

    m_pTaskBroker->Start();

    if (!pFrame)
        return UMC::UMC_OK;

    //DEBUG_PRINT((VM_STRING("Decode POC - %d\n"), pFrame->m_PicOrderCnt[0]));

    return UMC::UMC_OK;
}

// Retrieve decoded SEI data with SEI_USER_DATA_REGISTERED_TYPE type
UMC::Status TaskSupplier_MPEG2::GetUserData(UMC::MediaData * pUD)
{
    if(!pUD)
        return UMC::UMC_ERR_NULL_PTR;

    if (!m_pLastDisplayed)
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    if (m_pLastDisplayed->m_UserData.user_data.size() && m_pLastDisplayed->m_UserData.payLoadSize &&
        m_pLastDisplayed->m_UserData.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
    {
        pUD->SetTime(m_pLastDisplayed->m_dFrameTime);
        pUD->SetBufferPointer(&m_pLastDisplayed->m_UserData.user_data[0],
            m_pLastDisplayed->m_UserData.payLoadSize);
        pUD->SetDataSize(m_pLastDisplayed->m_UserData.payLoadSize);
        //m_pLastDisplayed->m_UserData.Reset();
        return UMC::UMC_OK;
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

bool TaskSupplier_MPEG2::IsShouldSuspendDisplay()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_iThreadNum <= 1)
        return true;

    ViewItem_MPEG2 &view = *GetView();

    if (view.pDPB->GetDisposable() || view.pDPB->countAllFrames() < view.dpbSize + m_DPBSizeEx)
        return false;

    return true;
}

// Find an index of specified level
uint32_t GetLevelIDCIndex(uint32_t level_idc)
{
    for (uint32_t i = 0; i < sizeof(levelIndexArray)/sizeof(levelIndexArray[0]); i++)
    {
        if (levelIndexArray[i] == level_idc)
            return i;
    }

    return sizeof(levelIndexArray)/sizeof(levelIndexArray[0]) - 1;
}

// Calculate maximum DPB size based on level and resolution
int32_t CalculateDPBSize(uint32_t /*profile_idc*/, uint32_t &level_idc, int32_t width, int32_t height, uint32_t num_ref_frames)
{
    // can increase level_idc to hold num_ref_frames
    uint32_t lumaPsArray[] = { 36864, 122880, 245760, 552960, 983040, 2228224, 2228224, 8912896, 8912896, 8912896, 35651584, 35651584, 35651584 };
    uint32_t MaxDpbSize = 16;

    for (;;)
    {
        uint32_t index = GetLevelIDCIndex(level_idc);

        uint32_t MaxLumaPs = lumaPsArray[index];
        uint32_t const maxDpbPicBuf =
            6;//HW handles second version of current reference (twoVersionsOfCurrDecPicFlag) itself

        uint32_t PicSizeInSamplesY = width * height;

        if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  2 ))
            MaxDpbSize = MFX_MIN(4 * maxDpbPicBuf, 16);
        else if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  1 ))
            MaxDpbSize = MFX_MIN(2 * maxDpbPicBuf, 16);
        else if (PicSizeInSamplesY  <=  ((3 * MaxLumaPs)  >>  2 ))
            MaxDpbSize = MFX_MIN((4 * maxDpbPicBuf) / 3, 16);
        else
            MaxDpbSize = maxDpbPicBuf;

        if (num_ref_frames <= MaxDpbSize)
            break;


        if (index >= sizeof(levelIndexArray)/sizeof(levelIndexArray[0]) - 1)
            break;

        level_idc = levelIndexArray[index + 1];

    }

    return MaxDpbSize;
}

// Table 6-4 – frame_rate_value
void GetMfxFrameRate(uint8_t frame_rate_value, mfxU32 & frameRateN, mfxU32 & frameRateD)
{
    switch (frame_rate_value)
    {
        case 0:  frameRateN = 30;    frameRateD = 1;    break;
        case 1:  frameRateN = 24000; frameRateD = 1001; break;
        case 2:  frameRateN = 24;    frameRateD = 1;    break;
        case 3:  frameRateN = 25;    frameRateD = 1;    break;
        case 4:  frameRateN = 30000; frameRateD = 1001; break;
        case 5:  frameRateN = 30;    frameRateD = 1;    break;
        case 6:  frameRateN = 50;    frameRateD = 1;    break;
        case 7:  frameRateN = 60000; frameRateD = 1001; break;
        case 8:  frameRateN = 60;    frameRateD = 1;    break;
        default: frameRateN = 30;    frameRateD = 1;
    }
    return;
}


// Table 8-3 – Level identification
mfxU8 GetMfxCodecLevel(uint8_t level)
{
    switch (level)
    {
        case 10:
            return MFX_LEVEL_MPEG2_LOW;
        case 8:
            return MFX_LEVEL_MPEG2_MAIN;
        case 6:
            return MFX_LEVEL_MPEG2_HIGH1440;
        case 4:
            return MFX_LEVEL_MPEG2_HIGH;
        default:
            return MFX_LEVEL_UNKNOWN;
    }
}
// Table 8-2 – Profile identification
mfxU8 GetMfxCodecProfile(uint8_t profile)
{
    switch (profile)
    {
        case 5:
            return MFX_PROFILE_MPEG2_SIMPLE;
        case 4:
            return MFX_PROFILE_MPEG2_MAIN;
        case 1:
            return MFX_PROFILE_MPEG2_HIGH;
        default:
            return MFX_PROFILE_UNKNOWN;
    }
}

bool DARtoPAR(uint32_t width, uint32_t height, uint32_t dar_h, uint32_t dar_v,
              uint16_t & par_h, uint16_t &par_v)
{
    uint32_t simple_tab[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59};

    uint32_t h = dar_h * height;
    uint32_t v = dar_v * width;

    // remove common multipliers
    while ( ((h|v)&1) == 0 )
    {
        h >>= 1;
        v >>= 1;
    }

    uint32_t denom;
    for (uint32_t i=0; i < (uint32_t)(sizeof(simple_tab)/sizeof(simple_tab[0])); ++i)
    {
        denom = simple_tab[i];
        while(h%denom==0 && v%denom==0)
        {
            v /= denom;
            h /= denom;
        }
        if (v <= denom || h <= denom)
            break;
    }
    par_h = h;
    par_v = v;

    return true;
}


bool CalcAspectRatio(uint32_t dar_code, uint32_t width, uint32_t height,
                    uint16_t & aspectRatioW, uint16_t & aspectRatioH)
{
    bool ret = true;

    if (dar_code == 2)
    {
        ret = DARtoPAR(width, height, 4, 3, aspectRatioW, aspectRatioH);
    }
    else if (dar_code == 3)
    {
        ret = DARtoPAR(width, height, 16, 9, aspectRatioW, aspectRatioH);
    }
    else if (dar_code == 4)
    {
        ret = DARtoPAR(width, height, 221, 100, aspectRatioW, aspectRatioH);
    }
    else // dar_code == 1 or unknown
    {
        aspectRatioW = 1;
        aspectRatioH = 1;
    }
    return ret;
}



} // namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
