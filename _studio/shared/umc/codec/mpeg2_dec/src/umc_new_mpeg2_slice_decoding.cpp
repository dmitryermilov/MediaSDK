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

#include "umc_new_mpeg2_slice_decoding.h"
#include "umc_new_mpeg2_heap.h"
#include "umc_new_mpeg2_frame_info.h"
#include "umc_new_mpeg2_frame_list.h"

#include <algorithm>
#include <numeric>

namespace UMC_MPEG2_DECODER
{
MPEG2Slice::MPEG2Slice()
    : m_pSeqParamSet(0)
    , m_context(0)
    , m_tileByteLocation (0)
{
    Reset();
} // MPEG2Slice::MPEG2Slice()

MPEG2Slice::~MPEG2Slice()
{
    Release();

} // MPEG2Slice::~MPEG2Slice(void)

// Initialize slice structure to default values
void MPEG2Slice::Reset()
{
    m_source.Release();

    if (m_pSeqParamSet)
    {
        if (m_pSeqParamSet)
            ((MPEG2SeqParamSet*)m_pSeqParamSet)->DecrementReference();
        if (m_pPicParamSet)
            ((MPEG2PicParamSet*)m_pPicParamSet)->DecrementReference();
        m_pSeqParamSet = 0;
        m_pPicParamSet = 0;
    }

    m_pCurrentFrame = 0;

    m_SliceHeader.nuh_temporal_id = 0;
    m_SliceHeader.m_CheckLDC = false;
    m_SliceHeader.slice_deblocking_filter_disabled_flag = false;
    m_SliceHeader.num_entry_point_offsets = 0;

    m_tileCount = 0;
    delete[] m_tileByteLocation;
    m_tileByteLocation = 0;
}

// Release resources
void MPEG2Slice::Release()
{
    Reset();
} // void MPEG2Slice::Release(void)

// Parse beginning of slice header to get PPS ID
int32_t MPEG2Slice::RetrievePicParamSetNumber()
{
    if (!m_source.GetDataSize())
        return -1;

    memset(reinterpret_cast<void*>(&m_SliceHeader), 0, sizeof(m_SliceHeader));
    m_BitStream.Reset((uint8_t *) m_source.GetPointer(), (uint32_t) m_source.GetDataSize());

    UMC::Status umcRes = UMC::UMC_OK;

    try
    {
        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nuh_temporal_id);
        if (UMC::UMC_OK != umcRes)
            return false;

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_SliceHeader);
        if (UMC::UMC_OK != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_SliceHeader.slice_pic_parameter_set_id;
}

// Decode slice header and initializ slice structure with parsed values
bool MPEG2Slice::Reset(PocDecoding * pocDecoding)
{
    m_BitStream.Reset((uint8_t *) m_source.GetPointer(), (uint32_t) m_source.GetDataSize());

    // decode slice header
    if (m_source.GetDataSize() && false == DecodeSliceHeader())
        return false;

    m_SliceHeader_.m_HeaderBitstreamOffset = (uint32_t)m_BitStream.BytesDecoded();
    m_SliceHeader_.m_MbOffset = (uint32_t)m_BitStream.BitsDecoded();

    m_SliceHeader_.m_SequenceParam      = m_pSequenceParam;
    m_SliceHeader_.m_SequenceParamExt   = m_pSequenceParamExt;
    //m_SliceHeader_.m_SequenceDisplayExt =
    m_SliceHeader_.m_PictureParam       = m_pPictureParam;

    //int32_t iMBInFrame = (GetSeqParam()->WidthInCU * GetSeqParam()->HeightInCU);

    // set slice variables
    //m_iFirstMB = m_SliceHeader.slice_segment_address;
    //m_iFirstMB = m_iFirstMB > iMBInFrame ? iMBInFrame : m_iFirstMB;
    //m_iFirstMB = m_pPicParamSet->m_CtbAddrRStoTS[m_iFirstMB];
    //m_iMaxMB = iMBInFrame;

    //processInfo.Initialize(m_iFirstMB, GetSeqParam()->WidthInCU);

    m_bError = false;

    // frame is not associated yet
    m_pCurrentFrame = NULL;
    return true;

} // bool MPEG2Slice::Reset(void *pSource, size_t nSourceSize, int32_t iNumber)

// Set current slice number
void MPEG2Slice::SetSliceNumber(int32_t iSliceNumber)
{
    m_iNumber = iSliceNumber;

} // void MPEG2Slice::SetSliceNumber(int32_t iSliceNumber)

// Decoder slice header and calculate POC
bool MPEG2Slice::DecodeSliceHeader()
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    try
    {
        memset(reinterpret_cast<void*>(&m_SliceHeader_), 0, sizeof(m_SliceHeader_));
/*
        NalUnitType nal_unit_type;
        umcRes = m_BitStream.GetNALUnitType(nal_unit_type);
        if (UMC::UMC_OK != umcRes)
            return false;

        if (nal_unit_type < 0x1 || nal_unit_type > 0xAF)
            return false;
*/
        umcRes = m_BitStream.GetSliceHeaderFull(this, m_pSequenceParam, m_pSequenceParam->GetSeqExt());

    }
    catch(...)
    {
        return false;
    }

    return (UMC::UMC_OK == umcRes);

}

// Decoder slice header and calculate POC
bool MPEG2Slice::DecodeSliceHeader(PocDecoding * pocDecoding)
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    try
    {
        memset(reinterpret_cast<void*>(&m_SliceHeader), 0, sizeof(m_SliceHeader));

        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nuh_temporal_id);
        if (UMC::UMC_OK != umcRes)
            return false;

       // umcRes = m_BitStream.GetSliceHeaderFull(this, m_pSeqParamSet, m_pPicParamSet);

        if (!GetSliceHeader()->dependent_slice_segment_flag)
        {
            if (!GetSliceHeader()->IdrPicFlag)
            {
                int32_t PicOrderCntMsb;
                int32_t slice_pic_order_cnt_lsb = m_SliceHeader.slice_pic_order_cnt_lsb;
                int32_t MaxPicOrderCntLsb = 1<< GetSeqParam()->log2_max_pic_order_cnt_lsb;
                int32_t prevPicOrderCntLsb = pocDecoding->prevPocPicOrderCntLsb;
                int32_t prevPicOrderCntMsb = pocDecoding->prevPicOrderCntMsb;

                if ( (slice_pic_order_cnt_lsb  <  prevPicOrderCntLsb) && ( (prevPicOrderCntLsb - slice_pic_order_cnt_lsb)  >=  (MaxPicOrderCntLsb / 2) ) )
                {
                    PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
                }
                else if ( (slice_pic_order_cnt_lsb  >  prevPicOrderCntLsb) && ( (slice_pic_order_cnt_lsb - prevPicOrderCntLsb)  >  (MaxPicOrderCntLsb / 2) ) )
                {
                    PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
                }
                else
                {
                    PicOrderCntMsb = prevPicOrderCntMsb;
                }

                if (m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP || m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL ||
                    m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP)
                { // For BLA picture types, POCmsb is set to 0.

                    PicOrderCntMsb = 0;
                }

                m_SliceHeader.slice_pic_order_cnt_lsb = PicOrderCntMsb + slice_pic_order_cnt_lsb;

                const MPEG2SeqParamSet * sps = m_pSeqParamSet;
                MPEG2SliceHeader * sliceHdr = &m_SliceHeader;

                if (GetSeqParam()->long_term_ref_pics_present_flag)
                {
                    ReferencePictureSet *rps = getRPS();
                    uint32_t offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

                    int32_t prevDeltaMSB = 0;
                    int32_t maxPicOrderCntLSB = 1 << sps->log2_max_pic_order_cnt_lsb;
                    int32_t DeltaPocMsbCycleLt = 0;
                    for(uint32_t j = offset, k = 0; k < rps->getNumberOfLongtermPictures(); j++, k++)
                    {
                        int pocLsbLt = rps->poc_lbs_lt[j];
                        if (rps->delta_poc_msb_present_flag[j])
                        {
                            bool deltaFlag = false;

                            if( (j == offset) || (j == (offset + rps->num_long_term_sps)))
                                deltaFlag = true;

                            if(deltaFlag)
                                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j];
                            else
                                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j] + prevDeltaMSB;

                            int32_t pocLTCurr = sliceHdr->slice_pic_order_cnt_lsb - DeltaPocMsbCycleLt * maxPicOrderCntLSB - slice_pic_order_cnt_lsb + pocLsbLt;
                            rps->setPOC(j, pocLTCurr);
                            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLTCurr);
                        }
                        else
                        {
                            rps->setPOC     (j, pocLsbLt);
                            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLsbLt);
                            if (j == offset + rps->num_long_term_sps)
                                DeltaPocMsbCycleLt = 0;
                        }

                        prevDeltaMSB = DeltaPocMsbCycleLt;
                    }

                    offset += rps->getNumberOfLongtermPictures();
                    rps->num_pics = offset;
                }

                if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
                    || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
                    || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP )
                {
                    ReferencePictureSet *rps = getRPS();
                    rps->num_negative_pics = 0;
                    rps->num_positive_pics = 0;
                    rps->setNumberOfLongtermPictures(0);
                    rps->num_pics = 0;
                }

                if (GetSliceHeader()->nuh_temporal_id == 0 && sliceHdr->nal_unit_type != NAL_UT_CODED_SLICE_RADL_R &&
                    sliceHdr->nal_unit_type != NAL_UT_CODED_SLICE_RASL_R && !IsSubLayerNonReference(sliceHdr->nal_unit_type))
                {
                    pocDecoding->prevPicOrderCntMsb = PicOrderCntMsb;
                    pocDecoding->prevPocPicOrderCntLsb = slice_pic_order_cnt_lsb;
                }
            }
            else
            {
                if (GetSliceHeader()->nuh_temporal_id == 0)
                {
                    pocDecoding->prevPicOrderCntMsb = 0;
                    pocDecoding->prevPocPicOrderCntLsb = 0;
                }
            }
        }

   }
    catch(...)
    {
	if (!m_SliceHeader.dependent_slice_segment_flag)
        {
            if (m_SliceHeader.slice_type != I_SLICE)
                m_bError = true;
        }

        return false;
    }

    return (UMC::UMC_OK == umcRes);

} // bool MPEG2Slice::DecodeSliceHeader(bool bFullInitialization)


// Returns number of used references in RPS
int MPEG2Slice::getNumRpsCurrTempList() const
{
  int numRpsCurrTempList = 0;

  if (GetSliceHeader()->slice_type != I_SLICE)
  {
      const ReferencePictureSet *rps = getRPS();

      for(uint32_t i=0;i < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures();i++)
      {
        if(rps->getUsed(i))
        {
          numRpsCurrTempList++;
        }
      }
  }

  return numRpsCurrTempList;
}

// For dependent slice copy data from another slice
void MPEG2Slice::CopyFromBaseSlice(const MPEG2Slice * s)
{
    if (!s || !m_SliceHeader.dependent_slice_segment_flag)
        return;

    VM_ASSERT(s);
    m_iNumber = s->m_iNumber;

    const MPEG2SliceHeader * slice = s->GetSliceHeader();

    m_SliceHeader.IdrPicFlag = slice->IdrPicFlag;
    m_SliceHeader.slice_pic_order_cnt_lsb = slice->slice_pic_order_cnt_lsb;
    m_SliceHeader.nal_unit_type = slice->nal_unit_type;
    m_SliceHeader.SliceQP = slice->SliceQP;

    m_SliceHeader.slice_deblocking_filter_disabled_flag   = slice->slice_deblocking_filter_disabled_flag;
    m_SliceHeader.deblocking_filter_override_flag = slice->deblocking_filter_override_flag;
    m_SliceHeader.slice_beta_offset = slice->slice_beta_offset;
    m_SliceHeader.slice_tc_offset = slice->slice_tc_offset;

    for (int32_t i = 0; i < 3; i++)
    {
        m_SliceHeader.m_numRefIdx[i]     = slice->m_numRefIdx[i];
    }

    m_SliceHeader.m_CheckLDC            = slice->m_CheckLDC;
    m_SliceHeader.slice_type            = slice->slice_type;
    m_SliceHeader.slice_qp_delta        = slice->slice_qp_delta;
    m_SliceHeader.slice_cb_qp_offset    = slice->slice_cb_qp_offset;
    m_SliceHeader.slice_cr_qp_offset    = slice->slice_cr_qp_offset;

    m_SliceHeader.m_rps                     = slice->m_rps;
    m_SliceHeader.collocated_from_l0_flag   = slice->collocated_from_l0_flag;
    m_SliceHeader.collocated_ref_idx        = slice->collocated_ref_idx;
    m_SliceHeader.nuh_temporal_id           = slice->nuh_temporal_id;

    for ( int32_t e = 0; e < 2; e++ )
    {
        for ( int32_t n = 0; n < MAX_NUM_REF_PICS; n++ )
        {
            MFX_INTERNAL_CPY(m_SliceHeader.pred_weight_table[e][n], slice->pred_weight_table[e][n], sizeof(wpScalingParam)*3);
        }
    }

    m_SliceHeader.luma_log2_weight_denom = slice->luma_log2_weight_denom;
    m_SliceHeader.chroma_log2_weight_denom = slice->chroma_log2_weight_denom;
    m_SliceHeader.slice_sao_luma_flag = slice->slice_sao_luma_flag;
    m_SliceHeader.slice_sao_chroma_flag = slice->slice_sao_chroma_flag;
    m_SliceHeader.cabac_init_flag        = slice->cabac_init_flag;

    m_SliceHeader.mvd_l1_zero_flag = slice->mvd_l1_zero_flag;
    m_SliceHeader.slice_loop_filter_across_slices_enabled_flag  = slice->slice_loop_filter_across_slices_enabled_flag;
    m_SliceHeader.slice_temporal_mvp_enabled_flag                = slice->slice_temporal_mvp_enabled_flag;
    m_SliceHeader.max_num_merge_cand               = slice->max_num_merge_cand;

    m_SliceHeader.cu_chroma_qp_offset_enabled_flag = slice->cu_chroma_qp_offset_enabled_flag;

    // Set the start of real slice, not slice segment
    m_SliceHeader.SliceCurStartCUAddr = slice->SliceCurStartCUAddr;

    m_SliceHeader.m_RefPicListModification = slice->m_RefPicListModification;
}

// RPS data structure constructor
ReferencePictureSet::ReferencePictureSet()
{
  ::memset(this, 0, sizeof(*this));
}

// Bubble sort RPS by delta POCs placing negative values first, positive values second, increasing POS deltas
// Negative values are stored with biggest absolute value first. See MPEG2 spec 8.3.2.
void ReferencePictureSet::sortDeltaPOC()
{
    for (uint32_t j = 1; j < num_pics; j++)
    {
        int32_t deltaPOC = m_DeltaPOC[j];
        uint8_t Used = used_by_curr_pic_flag[j];
        for (int32_t k = j - 1; k >= 0; k--)
        {
            int32_t temp = m_DeltaPOC[k];
            if (deltaPOC < temp)
            {
                m_DeltaPOC[k + 1] = temp;
                used_by_curr_pic_flag[k + 1] = used_by_curr_pic_flag[k];
                m_DeltaPOC[k] = deltaPOC;
                used_by_curr_pic_flag[k] = Used;
            }
        }
    }
    int32_t NumNegPics = (int32_t) num_negative_pics;
    for (int32_t j = 0, k = NumNegPics - 1; j < NumNegPics >> 1; j++, k--)
    {
        int32_t deltaPOC = m_DeltaPOC[j];
        uint8_t Used = used_by_curr_pic_flag[j];
        m_DeltaPOC[j] = m_DeltaPOC[k];
        used_by_curr_pic_flag[j] = used_by_curr_pic_flag[k];
        m_DeltaPOC[k] = deltaPOC;
        used_by_curr_pic_flag[k] = Used;
    }
}

uint32_t ReferencePictureSet::getNumberOfUsedPictures() const
{
    uint32_t const total =
        std::accumulate(used_by_curr_pic_flag, used_by_curr_pic_flag + num_pics, 0);

    return total;
}

// RPS list data structure constructor
ReferencePictureSetList::ReferencePictureSetList()
    : m_NumberOfReferencePictureSets(0)
{
}

// Allocate RPS list data structure with a new number of RPS
void ReferencePictureSetList::allocate(unsigned NumberOfReferencePictureSets)
{
    if (m_NumberOfReferencePictureSets == NumberOfReferencePictureSets)
        return;

    m_NumberOfReferencePictureSets = NumberOfReferencePictureSets;
    referencePictureSet.resize(NumberOfReferencePictureSets);
}

} // namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
