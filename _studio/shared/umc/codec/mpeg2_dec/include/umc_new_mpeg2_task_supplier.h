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

#ifndef __UMC_MPEG2_TASK_SUPPLIER_H
#define __UMC_MPEG2_TASK_SUPPLIER_H

#include <vector>
#include <list>
#include "umc_new_mpeg2_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_new_mpeg2_heap.h"
#include "umc_new_mpeg2_frame_info.h"
#include "umc_new_mpeg2_frame_list.h"

#include "umc_new_mpeg2_headers.h"
#include "umc_frame_allocator.h"

#include "umc_new_mpeg2_au_splitter.h"
#include "umc_new_mpeg2_segment_decoder_base.h"

#include "umc_va_base.h"


namespace UMC_MPEG2_DECODER
{
class TaskBroker_MPEG2;

class MPEG2DBPList;
class MPEG2DecoderFrame;
class MPEG2Slice;
class MediaData;

class BaseCodecParams;
class MPEG2SegmentDecoderMultiThreaded;
class TaskBrokerSingleThreadDXVA;

class MemoryAllocator;

enum
{
    BASE_VIEW                   = 0,
    INVALID_VIEW_ID             = -1
};

/****************************************************************************************************/
// Skipping_MPEG2 class routine
/****************************************************************************************************/
class Skipping_MPEG2
{
public:
    Skipping_MPEG2();
    virtual
    ~Skipping_MPEG2();

    // Disable deblocking filter to increase performance
    void PermanentDisableDeblocking(bool disable);
    // Check if deblocking should be skipped
    bool IsShouldSkipDeblocking(MPEG2DecoderFrame * pFrame);
    // Check if frame should be skipped to decrease decoding delays
    bool IsShouldSkipFrame(MPEG2DecoderFrame * pFrame);
    // Set decoding skip frame mode
    void ChangeVideoDecodingSpeed(int32_t& num);
    void Reset();

    struct SkipInfo
    {
        bool isDeblockingTurnedOff;
        int32_t numberOfSkippedFrames;
    };

    // Get current skip mode state
    SkipInfo GetSkipInfo() const;

private:

    int32_t m_VideoDecodingSpeed;
    int32_t m_SkipCycle;
    int32_t m_ModSkipCycle;
    int32_t m_PermanentTurnOffDeblocking;
    int32_t m_SkipFlag;

    int32_t m_NumberOfSkippedFrames;
};

/****************************************************************************************************/
// TaskSupplier_MPEG2
/****************************************************************************************************/
class SEI_Storer_MPEG2
{
public:

    struct SEI_Message
    {
        MPEG2DecoderFrame * frame;

        size_t      size;
        size_t      offset;
        uint8_t*      data;

        int32_t      nal_type;
        double      timestamp;
        SEI_TYPE    type;

        int32_t      isUsed;

        SEI_Message()
        {
            clear();
        }

        bool empty() const
        { return !isUsed; }

        void clear()
        {
            frame = NULL;
            data =  NULL;
            size = offset = 0;
            nal_type = NAL_UT_INVALID;
            timestamp = 0;
            type = SEI_RESERVED;
            isUsed = 0;
        }
    };

    SEI_Storer_MPEG2();

    virtual ~SEI_Storer_MPEG2();

    // Initialize SEI storage
    void Init();

    // Deallocate SEI storage
    void Close();

    // Reset SEI storage
    void Reset();

    // Set timestamp for stored SEI messages
    void SetTimestamp(MPEG2DecoderFrame * frame);

    // Put a new SEI message to the storage
    SEI_Message* AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type);

    // Retrieve a stored SEI message which was not retrieved before
    const SEI_Message * GetPayloadMessage();

    // Set SEI frame for stored SEI messages
    void SetFrame(MPEG2DecoderFrame * frame);

private:

    enum
    {
        MAX_BUFFERED_SIZE = 16 * 1024, // 16 kb
        START_ELEMENTS = 10,
        MAX_ELEMENTS = 128
    };

    std::vector<uint8_t>  m_data;
    std::vector<SEI_Message> m_payloads;

    size_t m_offset;
    int32_t m_lastUsed;

    //std::list<> ;
};

/****************************************************************************************************/
// ViewItem_MPEG2 class routine
/****************************************************************************************************/
struct ViewItem_MPEG2
{
    // Default constructor
    ViewItem_MPEG2(void);

    // Copy constructor
    ViewItem_MPEG2(const ViewItem_MPEG2 &src);

    ~ViewItem_MPEG2();

    // Initialize the view, allocate resources
    UMC::Status Init();

    // Close the view and release all resources
    void Close(void);

    // Reset the view and reset all resource
    void Reset(void);

    // Reset the size of DPB for particular view item
    void SetDPBSize(MPEG2SeqParamSet *pSps, uint32_t & level_idc);

    // Pointer to the view's DPB
    mutable std::unique_ptr<MPEG2DBPList> pDPB;

    // Size of DPB capacity in frames
    uint32_t dpbSize;
    // Maximum number frames used semultaneously
    uint32_t sps_max_dec_pic_buffering;
    uint32_t sps_max_num_reorder_pics;

    // Pointer to the frame being processed
    MPEG2DecoderFrame *pCurFrame;

    double localFrameTime;
};

/****************************************************************************************************/
// MVC extension class routine
/****************************************************************************************************/
class MVC_Extension
{
public:
    MVC_Extension();
    virtual ~MVC_Extension();

    UMC::Status Init();
    virtual void Close();
    virtual void Reset();

    ViewItem_MPEG2 *GetView();

protected:
    uint32_t m_temporal_id;
    uint32_t m_priority_id;
    uint32_t HighestTid;

    uint32_t  m_level_idc;

    ViewItem_MPEG2 m_view;
};

/****************************************************************************************************/
// DecReferencePictureMarking_MPEG2
/****************************************************************************************************/
class DecReferencePictureMarking_MPEG2
{
public:

    DecReferencePictureMarking_MPEG2();

    // Update DPB contents marking frames for reuse
    UMC::Status UpdateRefPicMarking(ViewItem_MPEG2 &view, const MPEG2Slice * pSlice);

    void Reset();

    uint32_t  GetDPBError() const;

protected:

    uint32_t  m_isDPBErrorFound;
    int32_t  m_frameCount;

    void ResetError();
};

/****************************************************************************************************/
// Prepare data for asychronous processing
/****************************************************************************************************/
class TaskSupplier_MPEG2 : public Skipping_MPEG2, public AU_Splitter_MPEG2, public MVC_Extension, public DecReferencePictureMarking_MPEG2
{
    friend class TaskBroker_MPEG2;
    friend class TaskBrokerSingleThreadDXVA;

public:
    uint32_t m_SliceIdxInTaskSupplier; //for mpeg2 sliceidx cursliceidx m_sliceidx m_currsliceidx m_inumber

    TaskSupplier_MPEG2();
    virtual ~TaskSupplier_MPEG2();

    // Initialize task supplier and creak task broker
    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    // create broker and segment decoders
    virtual void CreateTaskBroker();

    // Initialize what is necessary to decode bitstream header before the main part is initialized
    virtual UMC::Status PreInit(UMC::VideoDecoderParams *pInit);

    // Reset to default state
    virtual void Reset();
    // Release allocated resources
    virtual void Close();

    // Fill up current bitstream information
    UMC::Status GetInfo(UMC::VideoDecoderParams *lpInfo);

    // Add a new bitstream data buffer to decoding
    virtual UMC::Status AddSource(UMC::MediaData * pSource);


    // Chose appropriate processing action for specified NAL unit
    UMC::Status ProcessNalUnit(UMC::MediaDataEx *nalUnit);

    void SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
    }

    void SetFrameAllocator(UMC::FrameAllocator *pFrameAllocator)
    {
        m_pFrameAllocator = pFrameAllocator;
    }

    // Find a next frame ready to be output from decoder
    virtual MPEG2DecoderFrame *GetFrameToDisplayInternal(bool force);

    // Retrieve decoded SEI data with SEI_USER_DATA_REGISTERED_TYPE type
    UMC::Status GetUserData(UMC::MediaData * pUD);

    bool IsShouldSuspendDisplay();

    MPEG2DBPList *GetDPBList()
    {
        ViewItem_MPEG2 *pView = GetView();

        if (NULL == pView)
        {
            return NULL;
        }

        return pView->pDPB.get();
    }

    TaskBroker_MPEG2 * GetTaskBroker()
    {
        return m_pTaskBroker;
    }

    // Start asynchronous decoding
    virtual UMC::Status RunDecoding();
    // Find a decoder frame instance with specified surface ID
    virtual MPEG2DecoderFrame * FindSurface(UMC::FrameMemID id);

    // Set frame display time
    void PostProcessDisplayFrame(MPEG2DecoderFrame *pFrame);

    // Attempt to recover after something unexpectedly went wrong
    virtual void AfterErrorRestore();

    SEI_Storer_MPEG2 * GetSEIStorer() const { return m_sei_messages;}

    Headers * GetHeaders() { return &m_Headers;}

    inline const MPEG2SeqParamSet *GetCurrentSequence(void) const
    {
        return m_Headers.m_SeqParams.GetCurrentHeader();
    }

    // Decode slice header start, set slice links to SPS and PPS and correct tile offsets table if needed
    virtual MPEG2Slice * DecodeSliceHeader(UMC::MediaDataEx *nalUnit);

    Heap_Objects * GetObjHeap()
    {
        return &m_ObjHeap;
    }

protected:

    // Include a new slice into a set of frame slices
    void AddSliceToFrame(MPEG2DecoderFrame *pFrame, MPEG2Slice *pSlice);

    // Initialize scaling list data if needed
    void ActivateHeaders(MPEG2SeqParamSet *sps, MPEG2PicParamSet *pps);

    // Check whether this slice should be skipped because of random access conditions. MPEG2 spec 3.111
    bool IsSkipForCRAorBLA(const MPEG2Slice *pSlice);

    // Calculate NoRaslOutputFlag flag for specified slice
    void CheckCRAOrBLA(const MPEG2Slice *pSlice);

    // Try to find a reusable frame or allocate a new one and initialize it with slice parameters
    virtual MPEG2DecoderFrame *AllocateNewFrame(const MPEG2Slice *pSlice);
    // Initialize just allocated frame with slice parameters
    virtual UMC::Status InitFreeFrame(MPEG2DecoderFrame *pFrame, const MPEG2Slice *pSlice);
    // Initialize frame's counter and corresponding parameters
    virtual void InitFrameCounter(MPEG2DecoderFrame *pFrame, const MPEG2Slice *pSlice);

    // Add a new slice to frame
    UMC::Status AddSlice(MPEG2Slice * pSlice, bool force);
    // Check whether all slices for the frame were found
    virtual void CompleteFrame(MPEG2DecoderFrame * pFrame);
    // Mark frame as full with slices
    virtual void OnFullFrame(MPEG2DecoderFrame * pFrame);

    // Update DPB contents marking frames for reuse
    void DPBUpdate(const MPEG2Slice * slice);

    // Not implemented
    virtual void AddFakeReferenceFrame(MPEG2Slice*);
    virtual MPEG2DecoderFrame* AddSelfReferenceFrame(MPEG2Slice*);

    // Find NAL units in new bitstream buffer and process them
    virtual UMC::Status AddOneFrame(UMC::MediaData * pSource);

    // Allocate frame internals
    virtual UMC::Status AllocateFrameData(MPEG2DecoderFrame * pFrame, mfxSize dimensions, const MPEG2SeqParamSet* pSeqParamSet, const MPEG2PicParamSet *pPicParamSet);

    // Decode a bitstream header NAL unit
    virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *nalUnit);
    // Decode SEI NAL unit
    virtual UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

    // Search DPB for a frame which may be reused
    virtual MPEG2DecoderFrame *GetFreeFrame();

    // If a frame has all slices found, add it to asynchronous decode queue
    UMC::Status CompleteDecodedFrames(MPEG2DecoderFrame ** decoded);

    // Try to reset in case DPB has overflown
    void PreventDPBFullness();

    MPEG2SegmentDecoderBase **m_pSegmentDecoder;
    uint32_t m_iThreadNum;

    int32_t      m_maxUIDWhenWasDisplayed;
    double      m_local_delta_frame_time;
    bool        m_use_external_framerate;
    bool        m_decodedOrder;
    bool        m_checkCRAInsideResetProcess;

    MPEG2Slice * m_pLastSlice;

    MPEG2DecoderFrame *m_pLastDisplayed;

    UMC::MemoryAllocator *m_pMemoryAllocator;
    UMC::FrameAllocator  *m_pFrameAllocator;

    // Keep track of which parameter set is in use.
    bool              m_WaitForIDR;
    bool              m_prevSliceBroken;

    int32_t m_RA_POC;
    uint8_t  NoRaslOutputFlag;
    NalUnitType m_IRAPType;

    uint32_t            m_DPBSizeEx;
    int32_t            m_frameOrder;

    TaskBroker_MPEG2 * m_pTaskBroker;

    UMC::VideoDecoderParams     m_initializationParams;

    int32_t m_UIDFrameCounter;

    MPEG2SEIPayLoad m_UserData;
    SEI_Storer_MPEG2 *m_sei_messages;

    PocDecoding m_pocDecoding;

    bool m_isInitialized;

    UMC::Mutex m_mGuard;

private:
    // Decode video parameters set NAL unit
    UMC::Status xDecodeVPS(MPEG2HeadersBitstream *);
    // Decode sequence parameters set NAL unit
    UMC::Status xDecodeSPS(MPEG2HeadersBitstream *);
    // Decode picture parameters set NAL unit
    UMC::Status xDecodePPS(MPEG2HeadersBitstream *);

    TaskSupplier_MPEG2 & operator = (TaskSupplier_MPEG2 &)
    {
        return *this;

    } // TaskSupplier_MPEG2 & operator = (TaskSupplier_MPEG2 &)

};

// Calculate maximum DPB size based on level and resolution
extern int32_t CalculateDPBSize(uint32_t profile_idc, uint32_t &level_idc, int32_t width, int32_t height, uint32_t num_ref_frames);

} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_TASK_SUPPLIER_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
