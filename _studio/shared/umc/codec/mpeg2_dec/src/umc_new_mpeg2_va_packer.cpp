// Copyright (c) 2017-2018 Intel Corporation
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

#include "umc_new_mpeg2_va_packer.h"

#ifdef UMC_VA
#include "umc_new_mpeg2_task_supplier.h"
#endif

#include "mfx_ext_buffers.h"

using namespace UMC;

namespace UMC_MPEG2_DECODER
{

Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
{
    Packer * packer = 0;

        packer = new PackerVA(va);

    return packer;
}

Packer::Packer(UMC::VideoAccelerator * va)
    : m_va(va)
{
}

Packer::~Packer()
{
}

/****************************************************************************************************/
// VA linux packer implementation
/****************************************************************************************************/
PackerVA::PackerVA(VideoAccelerator * va)
    : Packer(va)
{
}

Status PackerVA::GetStatusReport(void * , size_t )
{
    return UMC_OK;
}

void PackerVA::PackPicParams(const MPEG2DecoderFrame *pCurrentFrame, MPEG2DecoderFrameInfo * pSliceInfo, TaskSupplier_MPEG2 *supplier)
{
    MPEG2Slice * pSlice                       = pSliceInfo->GetSlice(0);
    const MPEG2SliceHeader_* sliceHeader      = pSlice->GetSliceHeader_();
    const MPEG2SequenceHeader* seq            = pSlice->GetSeqHeader();
    const MPEG2PictureHeader*  pic            = pSlice->GetPicHeader();
    const MPEG2PictureCodingExtension* picExt = pic->GetPicExt();

    UMCVACompBuffer *picParamBuf;
    VAPictureParameterBufferMPEG2* picParam = (VAPictureParameterBufferMPEG2*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferMPEG2));
    if (!picParam)
        throw mpeg2_exception(UMC_ERR_FAILED);

    picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferMPEG2));
    memset(picParam, 0, sizeof(VAPictureParameterBufferMPEG2));

    const MPEG2DecoderFrame* pRefPic0 = pCurrentFrame->GetRefPicList(0);
    const MPEG2DecoderFrame* pRefPic1 = pCurrentFrame->GetRefPicList(1);

    picParam->horizontal_size = seq->horizontal_size_value;
    picParam->vertical_size   = seq->vertical_size_value;

    if (MPEG2_P_PICTURE == (FrameType)pCurrentFrame->m_FrameType && pRefPic0)
    {
        picParam->forward_reference_picture  = (VASurfaceID)m_va->GetSurfaceID(pRefPic0->GetFrameMID());
        picParam->backward_reference_picture = VA_INVALID_SURFACE;
    }
    else if (MPEG2_B_PICTURE == (FrameType)pCurrentFrame->m_FrameType && pRefPic0 && pRefPic1)
    {
        picParam->forward_reference_picture  = (VASurfaceID)m_va->GetSurfaceID(pRefPic0->GetFrameMID());
        picParam->backward_reference_picture = (VASurfaceID)m_va->GetSurfaceID(pRefPic1->GetFrameMID());
    }
    else
    {
        picParam->forward_reference_picture  = VA_INVALID_SURFACE;
        picParam->backward_reference_picture = VA_INVALID_SURFACE;
    }

    picParam->picture_coding_type = pic->picture_coding_type;
    for(uint8_t i = 0; i < 4; ++i)
    {
        picParam->f_code |= (picExt->f_code[i] << (12 - 4*i));
    }

    picParam->picture_coding_extension.bits.intra_dc_precision         = picExt->intra_dc_precision;
    picParam->picture_coding_extension.bits.picture_structure          = picExt->picture_structure;
    picParam->picture_coding_extension.bits.top_field_first            = picExt->top_field_first;
    picParam->picture_coding_extension.bits.frame_pred_frame_dct       = picExt->frame_pred_frame_dct;
    picParam->picture_coding_extension.bits.concealment_motion_vectors = picExt->concealment_motion_vectors;
    picParam->picture_coding_extension.bits.q_scale_type               = picExt->q_scale_type;
    picParam->picture_coding_extension.bits.intra_vlc_format           = picExt->intra_vlc_format;
    picParam->picture_coding_extension.bits.alternate_scan             = picExt->alternate_scan;
    picParam->picture_coding_extension.bits.repeat_first_field         = picExt->repeat_first_field;
    picParam->picture_coding_extension.bits.progressive_frame          = picExt->progressive_frame;
    picParam->picture_coding_extension.bits.is_first_field             = 1; //for now
}

void PackerVA::CreateSliceParamBuffer(MPEG2DecoderFrameInfo * sliceInfo)
{
    int32_t count = sliceInfo->GetSliceCount();

    UMCVACompBuffer *pSliceParamBuf;
    size_t sizeOfStruct = m_va->IsLongSliceControl() ? sizeof(VASliceParameterBufferMPEG2) : sizeof(VASliceParameterBufferBase);
    m_va->GetCompBuffer(VASliceParameterBufferType, &pSliceParamBuf, sizeOfStruct*(count));
    if (!pSliceParamBuf)
        throw mpeg2_exception(UMC_ERR_FAILED);

    pSliceParamBuf->SetNumOfItem(count);
}

void PackerVA::CreateSliceDataBuffer(MPEG2DecoderFrameInfo * sliceInfo)
{
    int32_t count = sliceInfo->GetSliceCount();

    int32_t size = 0;
    int32_t AlignedNalUnitSize = 0;

    for (int32_t i = 0; i < count; i++)
    {
        MPEG2Slice  * pSlice = sliceInfo->GetSlice(i);

        uint8_t *pNalUnit; //ptr to first byte of start code
        uint32_t NalUnitSize; // size of NAL unit in byte
        MPEG2HeadersBitstream *pBitstream = pSlice->GetBitStream();

        pBitstream->GetOrg((uint32_t**)&pNalUnit, &NalUnitSize);
        size += NalUnitSize + 4;
    }

    UMCVACompBuffer* compBuf;
    m_va->GetCompBuffer(VASliceDataBufferType, &compBuf, size);
    if (!compBuf)
        throw mpeg2_exception(UMC_ERR_FAILED);

    compBuf->SetDataSize(0);
}

bool PackerVA::PackSliceParams(MPEG2Slice *pSlice, uint32_t &sliceNum, bool isLastSlice)
{
    static uint8_t start_code_prefix[] = {0, 0, 1};

    MPEG2DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
    const MPEG2SliceHeader_ *sliceHeader = pSlice->GetSliceHeader_();

    UMCVACompBuffer* compBuf;
    VASliceParameterBufferMPEG2* sliceParams = (VASliceParameterBufferMPEG2*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBuf);
    if (!sliceParams)
        throw mpeg2_exception(UMC_ERR_FAILED);

    if (m_va->IsLongSliceControl())
    {
        sliceParams += sliceNum;
        memset(sliceParams, 0, sizeof(VASliceParameterBufferMPEG2));
    }
    else
    {
        sliceParams = (VASliceParameterBufferMPEG2*)((VASliceParameterBufferBase*)sliceParams + sliceNum);
        memset(sliceParams, 0, sizeof(VASliceParameterBufferBase));
    }

    uint32_t    rawDataSize = 0;
    const void* rawDataPtr = 0;

    pSlice->m_BitStream.GetOrg((uint32_t**)&rawDataPtr, &rawDataSize);

    sliceParams->slice_data_size = rawDataSize + sizeof(start_code_prefix);
    sliceParams->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

    uint8_t *sliceDataBuf = (uint8_t*)m_va->GetCompBuffer(VASliceDataBufferType, &compBuf);
    if (!sliceDataBuf)
        throw mpeg2_exception(UMC_ERR_FAILED);

    sliceParams->slice_data_offset = compBuf->GetDataSize();
    sliceDataBuf += sliceParams->slice_data_offset;
    MFX_INTERNAL_CPY(sliceDataBuf, start_code_prefix, sizeof(start_code_prefix));
    MFX_INTERNAL_CPY(sliceDataBuf + sizeof(start_code_prefix), rawDataPtr, rawDataSize);
    compBuf->SetDataSize(sliceParams->slice_data_offset + sliceParams->slice_data_size);

    if (!m_va->IsLongSliceControl())
        return true;

    sliceParams->macroblock_offset = sliceHeader->m_MbOffset + sizeof(start_code_prefix) * 8;
    sliceParams->slice_horizontal_position = sliceHeader->m_macroblock_address_increment - 1;
    sliceParams->slice_vertical_position = sliceHeader->slice_vertical_position - 1;
    sliceParams->quantiser_scale_code = sliceHeader->quantiser_scale_code;
    sliceParams->intra_slice_flag = sliceHeader->intra_slice_flag;

    return true;
}

static const uint8_t default_intra_quantizer_matrix[64] =
{
     8, 16, 16, 19, 16, 19, 22, 22,
    22, 22, 22, 22, 26, 24, 26, 27,
    27, 27, 26, 26, 26, 26, 27, 27,
    27, 29, 29, 29, 34, 34, 34, 29,
    29, 29, 27, 27, 29, 29, 32, 32,
    34, 34, 37, 38, 37, 35, 35, 34,
    35, 38, 38, 40, 40, 40, 48, 48,
    46, 46, 56, 56, 58, 69, 69, 83
};

void PackerVA::PackQmatrix(const MPEG2Slice *pSlice)
{
    UMCVACompBuffer *quantBuf;
    VAIQMatrixBufferMPEG2* qmatrix = (VAIQMatrixBufferMPEG2*)m_va->GetCompBuffer(VAIQMatrixBufferType, &quantBuf, sizeof(VAIQMatrixBufferMPEG2));
    if (!qmatrix)
        throw mpeg2_exception(UMC_ERR_FAILED);
    quantBuf->SetDataSize(sizeof(VAIQMatrixBufferMPEG2));

    const MPEG2SliceHeader_* sliceHeader = pSlice->GetSliceHeader_();
    const MPEG2SequenceHeader* seq       = pSlice->GetSeqHeader();

    qmatrix->load_intra_quantiser_matrix              = 1;
    qmatrix->load_non_intra_quantiser_matrix          = 1;
    qmatrix->load_chroma_intra_quantiser_matrix       = 1;
    qmatrix->load_chroma_non_intra_quantiser_matrix   = 1;

    if (seq->load_intra_quantiser_matrix)
    {
        for (uint8_t i=0; i < 64; ++i)
        {
            qmatrix->intra_quantiser_matrix[i] = seq->intra_quantiser_matrix[i];
            qmatrix->chroma_intra_quantiser_matrix[i] = seq->intra_quantiser_matrix[i];
        }
    }
    else
    {
        for (uint8_t i=0; i < 64; ++i)
        {
            qmatrix->intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
            qmatrix->chroma_intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
        }
    }
    if (seq->load_non_intra_quantiser_matrix)
    {
        for (uint8_t i=0; i < 64; ++i)
        {
            qmatrix->non_intra_quantiser_matrix[i] = seq->non_intra_quantiser_matrix[i];
            qmatrix->chroma_non_intra_quantiser_matrix[i] = seq->non_intra_quantiser_matrix[i];
        }
    }
    else
    {
        for (uint8_t i=0; i < 64; ++i)
        {
            qmatrix->non_intra_quantiser_matrix[i]        = 16;
            qmatrix->chroma_non_intra_quantiser_matrix[i] = 16;
        }
    }
}

void PackerVA::PackAU(const MPEG2DecoderFrame *frame, TaskSupplier_MPEG2 * supplier)
{
    MPEG2DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

    MPEG2Slice *pSlice = sliceInfo->GetSlice(0);
    const MPEG2SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    MPEG2DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    PackPicParams(pCurrentFrame, sliceInfo, supplier);

    PackQmatrix(pSlice);

    CreateSliceParamBuffer(sliceInfo);
    CreateSliceDataBuffer(sliceInfo);

    uint32_t sliceNum = 0;
    for (int32_t n = 0; n < sliceCount; n++)
    {
        PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
        sliceNum++;
    }

    Status s = m_va->Execute();
    if(s != UMC_OK)
        throw mpeg2_exception(s);
}

void PackerVA::BeginFrame(MPEG2DecoderFrame* /* frame */)
{
}

void PackerVA::EndFrame()
{
}


} // namespace UMC_MPEG2_DECODER

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
