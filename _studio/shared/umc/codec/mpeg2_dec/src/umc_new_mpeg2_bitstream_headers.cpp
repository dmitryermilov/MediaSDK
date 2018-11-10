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

#include "vm_debug.h"
#include "umc_new_mpeg2_bitstream_headers.h"
#include "umc_new_mpeg2_slice_decoding.h"
#include "umc_new_mpeg2_headers.h"

namespace UMC_MPEG2_DECODER
{

MPEG2BaseBitstream::MPEG2BaseBitstream()
{
    Reset(0, 0);
}

MPEG2BaseBitstream::MPEG2BaseBitstream(uint8_t * const pb, const uint32_t maxsize)
{
    Reset(pb, maxsize);
}

MPEG2BaseBitstream::~MPEG2BaseBitstream()
{
}

// Reset the bitstream with new data pointer
void MPEG2BaseBitstream::Reset(uint8_t * const pb, const uint32_t maxsize)
{
    m_pbs       = (uint32_t*)pb;
    m_pbsBase   = (uint32_t*)pb;
    m_bitOffset = 31;
    m_maxBsSize    = maxsize;

} // void Reset(uint8_t * const pb, const uint32_t maxsize)

// Reset the bitstream with new data pointer and bit offset
void MPEG2BaseBitstream::Reset(uint8_t * const pb, int32_t offset, const uint32_t maxsize)
{
    m_pbs       = (uint32_t*)pb;
    m_pbsBase   = (uint32_t*)pb;
    m_bitOffset = offset;
    m_maxBsSize = maxsize;

} // void Reset(uint8_t * const pb, int32_t offset, const uint32_t maxsize)

// Return bitstream array base address and size
void MPEG2BaseBitstream::GetOrg(uint32_t **pbs, uint32_t *size)
{
    *pbs       = m_pbsBase;
    *size      = m_maxBsSize;
}

// Set current decoding position
void MPEG2BaseBitstream::SetDecodedBytes(size_t nBytes)
{
    m_pbs = m_pbsBase + (nBytes / 4);
    m_bitOffset = 31 - ((int32_t) ((nBytes % sizeof(uint32_t)) * 8));
}

// Return current bitstream address and bit offset
void MPEG2BaseBitstream::GetState(uint32_t** pbs,uint32_t* bitOffset)
{
    *pbs       = m_pbs;
    *bitOffset = m_bitOffset;

}

// Set current bitstream address and bit offset
void MPEG2BaseBitstream::SetState(uint32_t* pbs, uint32_t bitOffset)
{
    m_pbs = pbs;
    m_bitOffset = bitOffset;
}

// Check that position in bitstream didn't move outside the limit
bool MPEG2BaseBitstream::CheckBSLeft()
{
    size_t bitsDecoded = BitsDecoded();
    return (bitsDecoded > m_maxBsSize*8);
}

// Check whether more data is present
bool MPEG2BaseBitstream::More_RBSP_Data()
{
    int32_t code, tmp;
    uint32_t* ptr_state = m_pbs;
    int32_t  bit_state = m_bitOffset;

    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    int32_t remaining_bytes = (int32_t)BytesLeft();

    if (remaining_bytes <= 0)
        return false;

    // get top bit, it can be "rbsp stop" bit
    GetNBits(m_pbs, m_bitOffset, 1, code);

    // get remain bits, which is less then byte
    tmp = (m_bitOffset + 1) % 8;

    if(tmp)
    {
        GetNBits(m_pbs, m_bitOffset, tmp, code);
        if ((code << (8 - tmp)) & 0x7f)    // most sig bit could be rbsp stop bit
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }
    }

    remaining_bytes = (int32_t)BytesLeft();

    // run through remain bytes
    while (0 < remaining_bytes)
    {
        GetNBits(m_pbs, m_bitOffset, 8, code);

        if (code)
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }

        remaining_bytes -= 1;
    }

    return false;
}

MPEG2HeadersBitstream::MPEG2HeadersBitstream()
    : MPEG2BaseBitstream()
{
}

MPEG2HeadersBitstream::MPEG2HeadersBitstream(uint8_t * const pb, const uint32_t maxsize)
    : MPEG2BaseBitstream(pb, maxsize)
{
}

/*********************************MPEG2******************************************/

void MPEG2HeadersBitstream::GetSequenceHeader(MPEG2SequenceHeader *seq)
{
    if (!seq)
        throw mpeg2_exception(UMC::UMC_ERR_NULL_PTR);

    // 6.2.2.1 Sequence header
    seq->horizontal_size_value = GetBits(12);
    seq->vertical_size_value = GetBits(12);
    seq->aspect_ratio_information = GetBits(4);
    if (0 == seq->aspect_ratio_information || seq->aspect_ratio_information > 4)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    seq->frame_rate_code = GetBits(4);
    seq->bit_rate_value = GetBits(18);
    uint8_t marker_bit = GetBits(1);
    if (0 == marker_bit) // shall be '1'. This bit prevents emulation of start codes.
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    seq->vbv_buffer_size_value = GetBits(10);
    seq->constrained_parameters_flag = GetBits(1);
    seq->load_intra_quantiser_matrix = GetBits(1);
    if (seq->load_intra_quantiser_matrix)
    {
        for (uint32_t i = 0; i < 64; ++i)
            seq->intra_quantiser_matrix[i] = GetBits(8);
    }
    seq->load_non_intra_quantiser_matrix = GetBits(1);
    if (seq->load_non_intra_quantiser_matrix)
    {
        for (uint32_t i = 0; i < 64; ++i)
            seq->non_intra_quantiser_matrix[i] = GetBits(8);
    }
}

void MPEG2HeadersBitstream::GetSequenceExtension(MPEG2SequenceExtension *seqExt)
{
    if (!seqExt)
        throw mpeg2_exception(UMC::UMC_ERR_NULL_PTR);

    // 6.2.2.3 Sequence extension
    seqExt->profile_and_level_indication = GetBits(8);
    seqExt->progressive_sequence = GetBits(1);
    seqExt->chroma_format = GetBits(2);
    if (0 == seqExt->chroma_format)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    seqExt->horizontal_size_extension = GetBits(2);
    seqExt->vertical_size_extension = GetBits(2);
    seqExt->bit_rate_extension = GetBits(12);

    uint8_t marker_bit = GetBits(1);
    if (0 == marker_bit) // shall be '1'. This bit prevents emulation of start codes.
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    seqExt->vbv_buffer_size_extension = GetBits(8);
    seqExt->low_delay = GetBits(1);
    seqExt->frame_rate_extension_n = GetBits(2);
    seqExt->frame_rate_extension_d = GetBits(5);
}

void MPEG2HeadersBitstream::GetSequenceDisplayExtension(MPEG2SequenceDisplayExtension *dispExt)
{
    if (!dispExt)
        throw mpeg2_exception(UMC::UMC_ERR_NULL_PTR);

    // 6.2.2.4 Sequence display extension
    dispExt->video_format = GetBits(3);
    dispExt->colour_description = GetBits(1);
    if (dispExt->colour_description)
    {
        dispExt->colour_primaries = GetBits(8);
        dispExt->transfer_characteristics = GetBits(8);
        dispExt->matrix_coefficients = GetBits(8);
    }

    dispExt->display_horizontal_size = GetBits(14);

    uint8_t marker_bit = Get1Bit();
    if (0 == marker_bit) // shall be '1'. This bit prevents emulation of start codes.
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    dispExt->display_vertical_size = GetBits(14);
}

void MPEG2HeadersBitstream::GetPictureHeader(MPEG2PictureHeader *pic)
{
    pic->temporal_reference = GetBits(10);

    pic->picture_coding_type = GetBits(3);
    if (pic->picture_coding_type > MPEG2_B_PICTURE || pic->picture_coding_type < MPEG2_I_PICTURE)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    pic->vbv_delay = GetBits(16);

    if (pic->picture_coding_type == MPEG2_P_PICTURE || pic->picture_coding_type == MPEG2_B_PICTURE)
    {
        pic->full_pel_forward_vector = GetBits(1);
        pic->forward_f_code = GetBits(3);
    }

    if (pic->picture_coding_type == MPEG2_B_PICTURE)
    {
        pic->full_pel_backward_vector = GetBits(1);
        pic->backward_f_code = GetBits(3);
    }
    /*
    while (GetBits(1))
    {
        uint8_t extra_information_picture = GetBits(8);
    }
    */
}

void MPEG2HeadersBitstream::GetPictureExtensionHeader(MPEG2PictureCodingExtension *picExt)
{
    picExt->f_code[0] = GetBits(4); // forward horizontal
    picExt->f_code[1] = GetBits(4); // forward vertical
    picExt->f_code[2] = GetBits(4); // backward horizontal
    picExt->f_code[3] = GetBits(4); // backward vertical
    picExt->intra_dc_precision = GetBits(2);
    picExt->picture_structure = GetBits(2);
    picExt->top_field_first = GetBits(1);
    picExt->frame_pred_frame_dct = GetBits(1);
    picExt->concealment_motion_vectors = GetBits(1);
    picExt->q_scale_type = GetBits(1);
    picExt->intra_vlc_format = GetBits(1);
    picExt->alternate_scan = GetBits(1);
    picExt->repeat_first_field = GetBits(1);
    picExt->chroma_420_type = GetBits(1);
    picExt->progressive_frame = GetBits(1);
    picExt->composite_display_flag = GetBits(1);
    if (picExt->composite_display_flag)
    {
        picExt->v_axis = GetBits(1);
        picExt->field_sequence = GetBits(3);
        picExt->sub_carrier = GetBits(1);
        picExt->burst_amplitude = GetBits(7);
        picExt->sub_carrier_phase = GetBits(8);
    }
}

void MPEG2HeadersBitstream::GetQuantMatrix(MPEG2QuantMatrix *q)
{
    q->load_intra_quantiser_matrix = GetBits(1);
    if (q->load_intra_quantiser_matrix)
    {
        for (uint8_t i= 0; i < 64; ++i)
        {
            q->intra_quantiser_matrix[i] = GetBits(8);
        }
    }
    q->load_non_intra_quantiser_matrix = GetBits(1);
    if (q->load_non_intra_quantiser_matrix)
    {
        for (uint8_t i= 0; i < 64; ++i)
        {
            q->non_intra_quantiser_matrix[i] = GetBits(8);
        }
    }
    q->load_chroma_intra_quantiser_matrix = GetBits(1);
    if (q->load_chroma_intra_quantiser_matrix)
    {
        for (uint8_t i= 0; i < 64; ++i)
        {
            q->chroma_intra_quantiser_matrix[i] = GetBits(8);
        }
    }
    q->load_chroma_non_intra_quantiser_matrix = GetBits(1);
    if (q->load_chroma_non_intra_quantiser_matrix)
    {
        for (uint8_t i= 0; i < 64; ++i)
        {
            q->chroma_non_intra_quantiser_matrix[i] = GetBits(8);
        }
    }
}

UMC::Status MPEG2HeadersBitstream::GetSliceHeader(MPEG2SliceHeader_ * sliceHdr, const MPEG2SequenceHeader *seq, const MPEG2SequenceExtension * seqExt)
{
    if (!sliceHdr)
        throw mpeg2_exception(UMC::UMC_ERR_NULL_PTR);

    sliceHdr->slice_vertical_position = GetBits(8);

    uint16_t vertical_size = seq->vertical_size_value | (seqExt->vertical_size_extension << 14);

    if (vertical_size > 2800)
    {
        sliceHdr->slice_vertical_position_extension = GetBits(3);
    }

    /* FIXME
    if((sequenceHeader.extension_start_code_ID[task_num] == SEQUENCE_SCALABLE_EXTENSION_ID) &&
        (sequenceHeader.scalable_mode[task_num] == DATA_PARTITIONING))
    {
        GET_TO9BITS(video->bs, 7, code)
        return UMC_ERR_UNSUPPORTED;
    }
    */
    sliceHdr->quantiser_scale_code = GetBits(5);
    if (!sliceHdr->quantiser_scale_code)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (Check1Bit())
    {
        sliceHdr->intra_slice_flag = GetBits(1);
        sliceHdr->intra_slice = GetBits(1);
        GetBits(7); // reserved_bits

        while (Check1Bit())
        {
            GetBits(1);  // extra_bit_slice
            GetBits(8);  // extra_information_slice
        }
    }

    GetBits(1);  // extra_bit_slice

    return UMC::UMC_OK;
}

struct VLCEntry
{
    int8_t value;
    int8_t length;
};

static const VLCEntry MBAddrIncrTabB1_1[16] =
{
    { -2 + 1, 48 - 48 }, { -2 + 1, 67 - 67 },
    { 2 + 5,   3 + 2 },  { 2 + 4,   3 + 2 },
    { 0 + 5,   1 + 3 },  { 4 + 1,   2 + 2 },
    { 3 + 1,   2 + 2 },  { 0 + 4,   3 + 1 },
    { 2 + 1,   2 + 1 },  { 0 + 3,   0 + 3 },
    { 1 + 2,   2 + 1 },  { 1 + 2,   1 + 2 },
    { 0 + 2,   2 + 1 },  { 1 + 1,   2 + 1 },
    { 0 + 2,   2 + 1 },  { 0 + 2,   2 + 1 },
};

static const VLCEntry MBAddrIncrTabB1_2[104] =
{
    { 18 + 15, 5 + 6 }, { 18 + 14, 0 + 11 }, { 12 + 19, 7 + 4 }, { 6 + 24, 7 + 4 },
    { 4 + 25, 4 + 7 },  { 1 + 27, 0 + 11 },  { 4 + 23, 2 + 9 },  { 15 + 11, 3 + 8 },
    { 22 + 3, 1 + 10 }, { 15 + 9, 2 + 9 },   { 4 + 19, 5 + 6 },  { 0 + 22, 7 + 4 },
    { 2 + 19, 3 + 7 },  { 2 + 19, 7 + 3 },   { 1 + 19, 0 + 10 }, { 15 + 5, 6 + 4 },
    { 11 + 8, 4 + 6 },  { 0 + 19, 7 + 3 },   { 9 + 9, 8 + 2 },   { 10 + 8, 4 + 6 },
    { 8 + 9, 7 + 3 },   { 0 + 17, 6 + 4 },   { 5 + 11, 4 + 6 },  { 12 + 4, 3 + 7 },
    { 8 + 7, 3 + 5 },   { 2 + 13, 1 + 7 },   { 13 + 2, 2 + 6 },  { 1 + 14, 4 + 4 },
    { 0 + 15, 6 + 2 },  { 5 + 10, 5 + 3 },   { 4 + 11, 0 + 8 },  { 6 + 9, 3 + 5 },
    { 5 + 9, 4 + 4 },   { 5 + 9, 4 + 4 },    { 12 + 2, 3 + 5 },  { 5 + 9, 2 + 6 },
    { 9 + 5, 2 + 6 },   { 1 + 13, 7 + 1 },   { 1 + 13, 3 + 5 },  { 5 + 9, 4 + 4 },
    { 0 + 13, 1 + 7 },  { 12 + 1, 5 + 3 },   { 1 + 12, 7 + 1 },  { 2 + 11, 4 + 4 },
    { 1 + 12, 6 + 2 },  { 3 + 10, 3 + 5 },   { 6 + 7, 2 + 6 },   { 9 + 4, 6 + 2 },
    { 7 + 5, 2 + 6 },   { 11 + 1, 0 + 8 },   { 6 + 6, 5 + 3 },   { 3 + 9, 0 + 8 },
    { 4 + 8, 2 + 6 },   { 4 + 8, 7 + 1 },    { 10 + 2, 1 + 7 },  { 4 + 8, 6 + 2 },
    { 4 + 7, 2 + 6 },   { 4 + 7, 7 + 1 },    { 9 + 2, 6 + 2 },   { 8 + 3, 0 + 8 },
    { 7 + 4, 6 + 2 },   { 5 + 6, 3 + 5 },    { 9 + 2, 3 + 5 },   { 8 + 3, 0 + 8 },
    { 3 + 7, 5 + 3 },   { 7 + 3, 4 + 4 },    { 9 + 1, 4 + 4 },   { 8 + 2, 3 + 5 },
    { 3 + 7, 5 + 3 },   { 9 + 1, 6 + 2 },    { 7 + 3, 0 + 8 },   { 7 + 3, 2 + 6 },
    { 6 + 3, 6 + 1 },   { 3 + 6, 1 + 6 },    { 7 + 2, 6 + 1 },   { 8 + 1, 3 + 4 },
    { 4 + 5, 5 + 2 },   { 7 + 2, 1 + 6 },    { 2 + 7, 3 + 4 },   { 7 + 2, 1 + 6 },
    { 7 + 2, 0 + 7 },   { 7 + 2, 5 + 2 },    { 8 + 1, 0 + 7 },   { 4 + 5, 3 + 4 },
    { 0 + 9, 1 + 6 },   { 0 + 9, 4 + 3 },    { 4 + 5, 2 + 5 },   { 8 + 1, 5 + 2 },
    { 5 + 3, 0 + 7 },   { 6 + 2, 1 + 6 },    { 6 + 2, 6 + 1 },   { 4 + 4, 1 + 6 },
    { 0 + 8, 6 + 1 },   { 2 + 6, 1 + 6 },    { 2 + 6, 3 + 4 },   { 3 + 5, 6 + 1 },
    { 2 + 6, 2 + 5 },   { 3 + 5, 6 + 1 },    { 2 + 6, 6 + 1 },   { 6 + 2, 4 + 3 },
    { 3 + 5, 0 + 7 },   { 0 + 8, 0 + 7 },    { 6 + 2, 3 + 4 },   { 3 + 5, 1 + 6 },
};

// Decode macroblock_address_increment
UMC::Status MPEG2HeadersBitstream::DecodeMBAddress(MPEG2SliceHeader_ * sliceHdr)
{
    uint32_t macroblock_address_increment = 0;
    uint32_t cc;

    for (;;)
    {
        cc = GetBits(11);
        UngetBits(11);

        if (cc >= 24)
            break;

        if (cc != 15)    // if not macroblock_stuffing
        {
            if (cc != 8) // if not macroblock_escape
            {
                sliceHdr->m_macroblock_address_increment = 1;
                return UMC::UMC_OK;
            }

            macroblock_address_increment += 33;
        }

        cc = GetBits(11);
    }

    if (cc >= 1024)
    {
        cc = GetBits(1);
        sliceHdr->m_macroblock_address_increment = macroblock_address_increment + 1;
        return UMC::UMC_OK;
    }

    uint32_t cc1;
    uint32_t length;

    if (cc >= 128)
    {
        cc >>= 6;
        length = MBAddrIncrTabB1_1[cc].length;
        cc1 = GetBits(length);
        sliceHdr->m_macroblock_address_increment = macroblock_address_increment + MBAddrIncrTabB1_1[cc].value;
        return UMC::UMC_OK;
    }

    cc -= 24;
    length = MBAddrIncrTabB1_2[cc].length;
    cc1 = GetBits(length);
    sliceHdr->m_macroblock_address_increment = macroblock_address_increment + MBAddrIncrTabB1_2[cc].value;
    return UMC::UMC_OK;
}

// Parse slice header part which contains PPS ID
UMC::Status MPEG2HeadersBitstream::GetSliceHeaderPart1(MPEG2SliceHeader * sliceHdr)
{
    if (!sliceHdr)
        throw mpeg2_exception(UMC::UMC_ERR_NULL_PTR);

    sliceHdr->IdrPicFlag = (sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP) ? 1 : 0;
    sliceHdr->first_slice_segment_in_pic_flag = Get1Bit();

    if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_CRA )
    {
        sliceHdr->no_output_of_prior_pics_flag = Get1Bit();
    }

    sliceHdr->slice_pic_parameter_set_id = (uint16_t)GetVLCElementU();

    if (sliceHdr->slice_pic_parameter_set_id > 63)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    return UMC::UMC_OK;
}

// Parse full slice header
UMC::Status MPEG2HeadersBitstream::GetSliceHeaderFull(MPEG2Slice *rpcSlice, const MPEG2SequenceHeader * seq, const MPEG2SequenceExtension * seqExt)
{
    if (!rpcSlice)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    UMC::Status sts = GetSliceHeader(rpcSlice->GetSliceHeader_(), seq, seqExt);
    if (UMC::UMC_OK != sts)
        return sts;

    rpcSlice->m_SliceHeader_.m_HeaderBitstreamOffset = (uint32_t)BytesDecoded();
    rpcSlice->m_SliceHeader_.m_MbOffset = (uint32_t)BitsDecoded();

    sts = DecodeMBAddress(rpcSlice->GetSliceHeader_());
    if (UMC::UMC_OK != sts)
        return sts;

    if (CheckBSLeft())
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);
    return UMC::UMC_OK;
}

UMC::Status MPEG2HeadersBitstream::GetNALUnitType(NalUnitType &nal_unit_type)
{
    nal_unit_type = (NalUnitType)GetBits(8);
    return UMC::UMC_OK;
}

// Read and return NAL unit type and NAL storage idc.
// Bitstream position is expected to be at the start of a NAL unit.
UMC::Status MPEG2HeadersBitstream::GetNALUnitType(NalUnitType &nal_unit_type, uint32_t &nuh_temporal_id)
{
    uint32_t forbidden_zero_bit = Get1Bit();
    if (forbidden_zero_bit)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    nal_unit_type = (NalUnitType)GetBits(6);
    uint32_t nuh_layer_id = GetBits(6);
    if (nuh_layer_id)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    uint32_t const nuh_temporal_id_plus1 = GetBits(3);
    if (!nuh_temporal_id_plus1)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    nuh_temporal_id = nuh_temporal_id_plus1 - 1;
    if (nuh_temporal_id)
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_RADL
            && nal_unit_type != NAL_UT_CODED_SLICE_BLA_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR_W_RADL
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_CRA
            && nal_unit_type != NAL_UT_VPS
            && nal_unit_type != NAL_UT_SPS
            && nal_unit_type != NAL_UT_EOS
            && nal_unit_type != NAL_UT_EOB );
    }
    else
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_TLA_R
            && nal_unit_type != NAL_UT_CODED_SLICE_TSA_N
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_R
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_N );
    }

    return UMC::UMC_OK;
}

// Read optional access unit delimiter from bitstream.
UMC::Status MPEG2HeadersBitstream::GetAccessUnitDelimiter(uint32_t &PicCodType)
{
    PicCodType = GetBits(3);
    return UMC::UMC_OK;
}    // GetAccessUnitDelimiter

} // namespace UMC_MPEG2_DECODER
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
