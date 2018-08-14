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

#include "umc_new_mpeg2_dec_defs.h"
#include "umc_new_mpeg2_task_supplier.h"
#include "umc_new_mpeg2_nal_spl.h"

#include "mfx_common_decode_int.h"


#include <functional>
#include <algorithm>
#include <iterator>

namespace UMC_MPEG2_DECODER { namespace MFX_Utility
{

// Check HW capabilities
bool IsNeedPartialAcceleration_MPEG2(mfxVideoParam* par, eMFXHWType /*type*/)
{
    if (!par)
        return false;

#if defined(MFX_VA_LINUX)
    if (par->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && par->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400)
        return true;

    if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P210 || par->mfx.FrameInfo.FourCC == MFX_FOURCC_NV16)
        return true;
#endif

    return false;
}

inline
mfxU16 MatchProfile(mfxU32 fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12: return MFX_PROFILE_MPEG2_MAIN;
    }

    return MFX_PROFILE_UNKNOWN;
}


inline
bool CheckGUID(VideoCORE * core, eMFXHWType type, mfxVideoParam const* param)
{
    (void)type;

    mfxVideoParam vp = *param;
    mfxU16 profile = vp.mfx.CodecProfile & 0xFF;
    if (profile == MFX_PROFILE_UNKNOWN)
    {
        profile = MatchProfile(vp.mfx.FrameInfo.FourCC);
        vp.mfx.CodecProfile |= profile; //preserve tier
    }

// #if defined (MFX_VA_LINUX)
//     if (core->IsGuidSupported(DXVA_ModeMPEG2_VLD_Main, &vp) != MFX_ERR_NONE)
//         return false;
//
//     //Linux doesn't check GUID, just [mfxVideoParam]
//     switch (profile)
//     {
//         case MFX_PROFILE_MPEG2_MAIN:
//         case MFX_PROFILE_MPEG2_MAINSP:
//         case MFX_PROFILE_MPEG2_MAIN10:
//             return true;
//     }
//
//     return false;
// #endif
    return true;
}

// Returns implementation platform
eMFXPlatform GetPlatform_MPEG2(VideoCORE * core, mfxVideoParam * par)
{
    (void)core;

    if (!par)
        return MFX_PLATFORM_SOFTWARE;

    eMFXPlatform platform = core->GetPlatformType();
    eMFXHWType typeHW = MFX_HW_UNKNOWN;
    typeHW = core->GetHWType();

    if (IsNeedPartialAcceleration_MPEG2(par, typeHW) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

    if (platform != MFX_PLATFORM_SOFTWARE && !CheckGUID(core, typeHW, par))
        platform = MFX_PLATFORM_SOFTWARE;

    return platform;
}

bool IsBugSurfacePoolApplicable(eMFXHWType hwtype, mfxVideoParam * par)
{
    (void)hwtype;

    if (par == NULL)
        return false;


    return false;
}

inline
mfxU16 QueryMaxProfile(eMFXHWType type)
{
    return true;
//     if (type < MFX_HW_SCL)
//         return MFX_PROFILE_MPEG2_MAIN;
//     else
//         return MFX_PROFILE_MPEG2_MAIN10;
}

inline
bool CheckChromaFormat(mfxU16 profile, mfxU16 format)
{
    VM_ASSERT(profile != MFX_PROFILE_UNKNOWN);
    VM_ASSERT(!(profile > MFX_PROFILE_MPEG2_HIGH));

    if (format > MFX_CHROMAFORMAT_YUV444)
        return false;

    struct supported_t
    {
        mfxU16 profile;
        mfxI8  chroma[4];
    } static const supported[] =
    {
        { MFX_PROFILE_MPEG2_SIMPLE, {                      -1, MFX_CHROMAFORMAT_YUV420,                      -1,                      -1 } },
        { MFX_PROFILE_MPEG2_MAIN,   {                      -1, MFX_CHROMAFORMAT_YUV420,                      -1,                      -1 } },
        { MFX_PROFILE_MPEG2_HIGH,   {                      -1, MFX_CHROMAFORMAT_YUV420,                      -1,                      -1 } },


    };

    supported_t const
        *f = supported,
        *l = f + sizeof(supported) / sizeof(supported[0]);
    for (; f != l; ++f)
        if (f->profile == profile)
            break;

    return
        f != l && (*f).chroma[format] != -1;
}

inline
bool CheckBitDepth(mfxU16 profile, mfxU16 bit_depth)
{
    VM_ASSERT(profile != MFX_PROFILE_UNKNOWN);
    VM_ASSERT(!(profile > MFX_PROFILE_MPEG2_HIGH));

    struct minmax_t
    {
        mfxU16 profile;
        mfxU8  lo, hi;
    } static const minmax[] =
    {
        { MFX_PROFILE_MPEG2_SIMPLE, 8,  8 },
        { MFX_PROFILE_MPEG2_MAIN,   8,  8 },
        { MFX_PROFILE_MPEG2_HIGH,   8,  8 },
    };

    minmax_t const
        *f = minmax,
        *l = f + sizeof(minmax) / sizeof(minmax[0]);
    for (; f != l; ++f)
        if (f->profile == profile)
            break;

    return
        f != l &&
        !(bit_depth < f->lo) &&
        !(bit_depth > f->hi)
        ;
}

inline
mfxU32 CalculateFourcc(mfxU16 codecProfile, mfxFrameInfo const* frameInfo)
{
    return MFX_FOURCC_NV12;

//     //map profile + chroma fmt + bit depth => fcc
//     //Main   - [4:2:0], [8] bit
//     //Main10 - [4:2:0], [8, 10] bit
//     //Extent - [4:2:0, 4:2:2, 4:4:4], [8, 10, 12, 16]
//
//     if (codecProfile > MFX_PROFILE_MPEG2_REXT &&
//         codecProfile != MPEG2_PROFILE_SCC)
//         return 0;
//
//     if (!CheckChromaFormat(codecProfile, frameInfo->ChromaFormat))
//         return 0;
//
//     if (!CheckBitDepth(codecProfile, frameInfo->BitDepthLuma))
//         return 0;
//     if (!CheckBitDepth(codecProfile, frameInfo->BitDepthChroma))
//         return 0;
//
//     mfxU16 bit_depth =
//        MFX_MAX(frameInfo->BitDepthLuma, frameInfo->BitDepthChroma);
//
//     //map chroma fmt & bit depth onto fourcc (NOTE: we currently don't support bit depth above 10 bit)
//     mfxU32 const map[][4] =
//     {
//             /* 8 bit */      /* 10 bit */
//         {               0,               0,               0, 0 }, //400
//         { MFX_FOURCC_NV12, MFX_FOURCC_P010,               0, 0 }, //420
//         {               0,               0,               0, 0 }, //422
//         {               0,               0,               0, 0 }, //444
//     };
//
//     VM_ASSERT(
//         (frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV400 ||
//          frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV420 ||
//          frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV422) &&
//         "Unsupported chroma format, should be validated before"
//     );
//
//     //align luma depth up to 2 (8-10-12 ...)
//     bit_depth = (bit_depth + 2 - 1) & ~(2 - 1);
//     VM_ASSERT(!(bit_depth & 1) && "Luma depth should be aligned up to 2");
//
//     VM_ASSERT(
//         (bit_depth ==  8 ||
//          bit_depth == 10) &&
//         "Unsupported bit depth, should be validated before"
//     );
//
//     mfxU16 const bit_depth_idx     = (bit_depth - 8) / 2;
//     mfxU16 const max_bit_depth_idx = sizeof(map) / sizeof(map[0]);
//
//     return bit_depth_idx < max_bit_depth_idx ?
//         map[frameInfo->ChromaFormat][bit_depth_idx] : 0;
}

inline
bool CheckFourcc(mfxU32 fourcc, mfxU16 codecProfile, mfxFrameInfo const* frameInfo)
{
    VM_ASSERT(frameInfo);
    mfxFrameInfo fi = *frameInfo;

    if (codecProfile == MFX_PROFILE_UNKNOWN)
        //no profile defined, try to derive it from FOURCC
        codecProfile = MatchProfile(fourcc);

    if (!fi.BitDepthLuma)
    {
        //no depth defined, derive it from FOURCC
        switch (fourcc)
        {
            case MFX_FOURCC_NV12:
            case MFX_FOURCC_NV16:
                fi.BitDepthLuma = 8;
                break;

            case MFX_FOURCC_P010:
            case MFX_FOURCC_P210:
                fi.BitDepthLuma = 10;
                break;

            default:
                return false;
        }
    }

    if (!fi.BitDepthChroma)
        fi.BitDepthChroma = fi.BitDepthLuma;

    return
        CalculateFourcc(codecProfile, &fi) == fourcc;
}

// Initialize mfxVideoParam structure based on decoded bitstream header values
UMC::Status FillVideoParam(const MPEG2SequenceHeader * seq,
                           const MPEG2SequenceExtension * seqExt,
                           const MPEG2SequenceDisplayExtension * dispExt,
                           mfxVideoParam *par, bool full)
{
    par->mfx.CodecId = MFX_CODEC_MPEG2;

    par->mfx.FrameInfo.CropX = 0;
    par->mfx.FrameInfo.CropY = 0;
    par->mfx.FrameInfo.CropW = seq->horizontal_size_value;
    par->mfx.FrameInfo.CropH = seq->vertical_size_value;

    par->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(par->mfx.FrameInfo.CropW, 16);
    par->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(par->mfx.FrameInfo.CropH, seqExt->progressive_sequence ? 16 : 32);

    par->mfx.FrameInfo.BitDepthLuma   = 8;
    par->mfx.FrameInfo.BitDepthChroma = 8;
    par->mfx.FrameInfo.Shift = 0;

    par->mfx.FrameInfo.PicStruct = seqExt->progressive_sequence  ? MFX_PICSTRUCT_PROGRESSIVE : MFX_PICSTRUCT_UNKNOWN;
    par->mfx.FrameInfo.ChromaFormat = seqExt->chroma_format == CHROMA_FORMAT_420 ?
                                        MFX_CHROMAFORMAT_YUV420 :
                                        (seqExt->chroma_format == CHROMA_FORMAT_422 ?
                                                MFX_CHROMAFORMAT_YUV422 :
                                                MFX_CHROMAFORMAT_YUV444);

    CalcAspectRatio(seq->aspect_ratio_information, seq->horizontal_size_value, seq->vertical_size_value,
                    par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);

    GetMfxFrameRate(seqExt->frame_rate_code, par->mfx.FrameInfo.FrameRateExtN, par->mfx.FrameInfo.FrameRateExtD);

    // Table 8-1 – Meaning of bits in profile_and_level_indication
    par->mfx.CodecProfile = GetMfxCodecProfile((seqExt->profile_and_level_indication >> 4) & 7); // [6:4] bits
    par->mfx.CodecLevel = GetMfxCodecLevel(seqExt->profile_and_level_indication & 0xF); // [3:0] bites

    par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    par->mfx.DecodedOrder = 0;

    // video signal section
    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal && dispExt)
    {
        videoSignal->VideoFormat              = (mfxU16)dispExt->video_format;
        videoSignal->ColourPrimaries          = (mfxU16)dispExt->colour_primaries;
        videoSignal->TransferCharacteristics  = (mfxU16)dispExt->transfer_characteristics;
        videoSignal->MatrixCoefficients       = (mfxU16)dispExt->matrix_coefficients;
        videoSignal->ColourDescriptionPresent = (mfxU16)dispExt->colour_description;
    }

    return UMC::UMC_OK;
}

// Helper class for gathering header NAL units
class HeadersAnalyzer
{
public:

    HeadersAnalyzer(TaskSupplier_MPEG2 * supplier)
        : m_isSeqFound(false)
        , m_isSeqExtFound(false)
        , m_isDisplayExtSearchRequired(false)
        , m_supplier(supplier)
        , m_lastSlice(0)
    {}

    virtual ~HeadersAnalyzer()
    {
        if (m_lastSlice)
            m_lastSlice->DecrementReference();
    }

    // Decode a memory buffer looking for header NAL units in it
    virtual UMC::Status DecodeHeader(UMC::MediaData* params, mfxBitstream *bs, mfxVideoParam *out);
    // Find headers nal units and parse them
    virtual UMC::Status ProcessNalUnit(UMC::MediaData * data);
    // Returns whether necessary headers are found
    virtual bool IsEnough() const
    { return m_isSeqFound && m_isSeqExtFound && !m_isDisplayExtSearchRequired; }
    // Returns whether necessary sequence display extension is required
    virtual bool IsDisplayExtRequired() const
    { return m_isDisplayExtSearchRequired; }

protected:

    bool m_isSeqFound;
    bool m_isSeqExtFound;
    bool m_isDisplayExtSearchRequired;

    TaskSupplier_MPEG2 * m_supplier;
    MPEG2Slice * m_lastSlice;
};

// Decode a memory buffer looking for header NAL units in it
UMC::Status HeadersAnalyzer::DecodeHeader(UMC::MediaData * data, mfxBitstream *bs, mfxVideoParam * par)
{
    if (!data)
        return UMC::UMC_ERR_NULL_PTR;

    m_isDisplayExtSearchRequired = (mfxExtVideoSignalInfo *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

    m_lastSlice = 0;

    MPEG2SequenceHeader* first_seq = 0;
    notifier0<MPEG2SequenceHeader> seq_guard(&MPEG2Slice::DecrementReference);

    UMC::Status umcRes = UMC::UMC_ERR_NOT_ENOUGH_DATA;
    for ( ; data->GetDataSize() > 3; )
    {
        m_supplier->GetNalUnitSplitter()->MoveToStartCode(data); // move data pointer to start code

        if (!m_isSeqFound) // move point to first start code
        {
            bs->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
            bs->DataLength = (mfxU32)data->GetDataSize();
        }

        umcRes = ProcessNalUnit(data);

        if (umcRes == UMC::UMC_ERR_UNSUPPORTED)
            umcRes = UMC::UMC_OK;

        if (umcRes != UMC::UMC_OK)
            break;

        if (!first_seq && m_isSeqFound)
        {
            first_seq = m_supplier->GetHeaders()->m_SequenceParam.GetCurrentHeader();
            VM_ASSERT(first_seq && "Current sequence header should be valid when [m_isSeqFound]");

            MFX_CHECK_NULL_PTR1(first_seq);

            first_seq->IncrementReference();
            seq_guard.Reset(first_seq);
        }

        if (IsEnough())
            break;
    }

    if (umcRes == UMC::UMC_ERR_SYNC) // move pointer
    {
        bs->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
        bs->DataLength = (mfxU32)data->GetDataSize();
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
    {
        bool isEOS = ((data->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_END_OF_STREAM) != 0) ||
            ((data->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME) == 0);
        if (isEOS)
        {
            return UMC::UMC_OK;
        }
    }

    if (IsEnough())
    {
        MPEG2SequenceHeader* last_seq = m_supplier->GetHeaders()->m_SequenceParam.GetCurrentHeader();
        if (first_seq && first_seq != last_seq)
            m_supplier->GetHeaders()->m_SequenceParam.AddHeader(first_seq);

        return UMC::UMC_OK;
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

// Find headers nal units and parse them
UMC::Status HeadersAnalyzer::ProcessNalUnit(UMC::MediaData * data)
{
    try
    {
        int32_t startCode = m_supplier->GetNalUnitSplitter()->CheckNalUnitType(data);

        bool needProcess = false;

        UMC::MediaDataEx *nalUnit = m_supplier->GetNalUnit(data);

        switch (startCode)
        {
        case NAL_UT_PICTURE_HEADER:
            // Display extension had to be before the first picture.
            // So stop looking for the display extension.
            m_isDisplayExtSearchRequired = false;
        case NAL_UT_USER_DATA:
        case NAL_UT_SEQUENCE_ERROR:
        case NAL_UT_SEQUENCE_END:
        case NAL_UT_GROUP:
        {

            if (IsEnough())
            {
                return UMC::UMC_OK;
            }
            else
                break; // skip nal unit
        }
        break;
        case NAL_UT_SEQUENCE_HEADER:
        case NAL_UT_EXTENSION:
            needProcess = true;
            break;

        default:
            break;
        };

        if (!nalUnit)
        {
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }

        if (needProcess)
        {
            try
            {
                UMC::Status umcRes = m_supplier->ProcessNalUnit(nalUnit);
                if (umcRes < UMC::UMC_OK)
                {
                    return UMC::UMC_OK;
                }
            }
            catch(mpeg2_exception& ex)
            {
                if (ex.GetStatus() != UMC::UMC_ERR_UNSUPPORTED)
                {
                    throw;
                }
            }

            switch (startCode)
            {
            case NAL_UT_SEQUENCE_HEADER:
                m_isSeqFound = true;
                break;

            case NAL_UT_EXTENSION:
            {
                // TODO: need to think of a better way to get a type of extension
                uint8_t const * const data = (uint8_t*)nalUnit->GetDataPointer();
                NalUnitTypeExt extId = (NalUnitTypeExt) (data[1] >> 4);

                switch (extId)
                {
                case NAL_UT_EXT_SEQUENCE_EXTENSION:
                    m_isSeqExtFound = true;
                    break;
                case NAL_UT_EXT_SEQUENCE_DISPLAY_EXTENSION:
                    m_isDisplayExtSearchRequired = false; // found
                    break;
                default:
                    m_isDisplayExtSearchRequired = false; // questionable: shall DISPLAY_EXTENSION be the first extension after SEQUENCE_EXTENSION ?
                    break;
                }
            }
            break;

            default:
                break;
            };

            return UMC::UMC_OK;
        }
    }
    catch(const mpeg2_exception & ex)
    {
        return ex.GetStatus();
    }

    return UMC::UMC_OK;
}

// Find bitstream header NAL units, parse them and fill application parameters structure
UMC::Status DecodeHeader(TaskSupplier_MPEG2 * supplier, UMC::VideoDecoderParams* params, mfxBitstream *bs, mfxVideoParam *out)
{
    UMC::Status umcRes = UMC::UMC_OK;

    if (!params->m_pData)
        return UMC::UMC_ERR_NULL_PTR;

    if (!params->m_pData->GetDataSize())
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    umcRes = supplier->PreInit(params);
    if (umcRes != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    HeadersAnalyzer headersDecoder(supplier);
    umcRes = headersDecoder.DecodeHeader(params->m_pData, bs, out);

    if (umcRes != UMC::UMC_OK)
        return umcRes;

    return
        umcRes = supplier->GetInfo(params);
}

// MediaSDK DECODE_Query API function
mfxStatus Query_MPEG2(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxVideoParam in1;
        MFX_INTERNAL_CPY(&in1, in, sizeof(mfxVideoParam));
        return MFX_Utility::Query_MPEG2(core, &in1, out, type);
    }

    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_MPEG2)
            out->mfx.CodecId = in->mfx.CodecId;

        //use [core :: GetHWType] instead of given argument [type]
        //because it may be unknown after [GetPlatform_MPEG2]
        mfxU16 profile = QueryMaxProfile(core->GetHWType());
        if (/*in->mfx.CodecProfile == MFX_PROFILE_MPEG2_MAINSP ||*/
            in->mfx.CodecProfile <= profile)
            out->mfx.CodecProfile = in->mfx.CodecProfile;
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN)
            profile = out->mfx.CodecProfile;

        mfxU32 const level =
            ExtractProfile(in->mfx.CodecLevel);

        switch (level)
        {
        case MFX_LEVEL_UNKNOWN:
        case MFX_LEVEL_MPEG2_LOW:
        case MFX_LEVEL_MPEG2_MAIN:
        case MFX_LEVEL_MPEG2_HIGH:
        case MFX_LEVEL_MPEG2_HIGH1440:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        if (in->mfx.NumThread < 128)
        {
            out->mfx.NumThread = in->mfx.NumThread;
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->AsyncDepth = in->AsyncDepth;

        out->mfx.DecodedOrder = in->mfx.DecodedOrder;

        if (in->mfx.DecodedOrder > 1)
        {
            sts = MFX_ERR_UNSUPPORTED;
            out->mfx.DecodedOrder = 0;
        }

        if (in->mfx.TimeStampCalc)
        {
            if (in->mfx.TimeStampCalc == 1)
                in->mfx.TimeStampCalc = out->mfx.TimeStampCalc;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.ExtendedPicStruct)
        {
            if (in->mfx.ExtendedPicStruct == 1)
                in->mfx.ExtendedPicStruct = out->mfx.ExtendedPicStruct;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        {
            uint32_t mask = in->IOPattern & 0xf0;
            if (mask == MFX_IOPATTERN_OUT_VIDEO_MEMORY || mask == MFX_IOPATTERN_OUT_SYSTEM_MEMORY || mask == MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                out->IOPattern = in->IOPattern;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.FourCC)
        {
            // mfxFrameInfo
            if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 ||
                in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 ||
                in->mfx.FrameInfo.FourCC == MFX_FOURCC_P210
                )
                out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV400 ||
            CheckChromaFormat(profile, in->mfx.FrameInfo.ChromaFormat))
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (in->mfx.FrameInfo.Width % 16 == 0 && in->mfx.FrameInfo.Width <= 16384)
            out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
        else
        {
            out->mfx.FrameInfo.Width = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.Height % 16 == 0 && in->mfx.FrameInfo.Height <= 16384)
            out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
        else
        {
            out->mfx.FrameInfo.Height = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if ((in->mfx.FrameInfo.Width || in->mfx.FrameInfo.Height) && !(in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Height))
        {
            out->mfx.FrameInfo.Width = 0;
            out->mfx.FrameInfo.Height = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        if ((in->mfx.FrameInfo.FrameRateExtN || in->mfx.FrameInfo.FrameRateExtD) && !(in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD))
        {
            out->mfx.FrameInfo.FrameRateExtN = 0;
            out->mfx.FrameInfo.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        {
            out->mfx.FrameInfo.AspectRatioW = 0;
            out->mfx.FrameInfo.AspectRatioH = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.BitDepthLuma = in->mfx.FrameInfo.BitDepthLuma;
        if (in->mfx.FrameInfo.BitDepthLuma && !CheckBitDepth(profile, in->mfx.FrameInfo.BitDepthLuma))
        {
            out->mfx.FrameInfo.BitDepthLuma = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.BitDepthChroma = in->mfx.FrameInfo.BitDepthChroma;
        if (in->mfx.FrameInfo.BitDepthChroma && !CheckBitDepth(profile, in->mfx.FrameInfo.BitDepthChroma))
        {
            out->mfx.FrameInfo.BitDepthChroma = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.FourCC &&
            !CheckFourcc(in->mfx.FrameInfo.FourCC, profile, &in->mfx.FrameInfo))
        {
            out->mfx.FrameInfo.FourCC = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.Shift = in->mfx.FrameInfo.Shift;
        if (   in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || in->mfx.FrameInfo.FourCC == MFX_FOURCC_P210
            )
        {
            if (in->mfx.FrameInfo.Shift > 1)
            {
                out->mfx.FrameInfo.Shift = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            if (in->mfx.FrameInfo.Shift)
            {
                out->mfx.FrameInfo.Shift = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_SINGLE:
            out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;


        if (GetPlatform_MPEG2(core, out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform_MPEG2(core, out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_MPEG2;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;

        out->mfx.NumThread = 1;

        out->mfx.DecodedOrder = 1;

        out->mfx.SliceGroupsPresent = 1;
        out->mfx.ExtendedPicStruct = 1;
        out->AsyncDepth = 1;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 16;
        out->mfx.FrameInfo.Height = 16;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;

        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;

        out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        out->mfx.FrameInfo.BitDepthLuma = 8;
        out->mfx.FrameInfo.BitDepthChroma = 8;
        out->mfx.FrameInfo.Shift = 0;

        out->Protected = 0;

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
    }

    return sts;
}

// Validate input parameters
bool CheckVideoParam_MPEG2(mfxVideoParam *in, eMFXHWType /* type */)
{
    if (!in)
        return false;

    if (MFX_CODEC_MPEG2 != in->mfx.CodecId)
        return false;

    // FIXME: Add check that width is multiple of minimal CU size
    if (in->mfx.FrameInfo.Width > 16384 /* || (in->mfx.FrameInfo.Width % in->mfx.FrameInfo.reserved[0]) */)
        return false;

    // FIXME: Add check that height is multiple of minimal CU size
    if (in->mfx.FrameInfo.Height > 16384 /* || (in->mfx.FrameInfo.Height % in->mfx.FrameInfo.reserved[0]) */)
        return false;


    if (in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
        in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV16 &&
        in->mfx.FrameInfo.FourCC != MFX_FOURCC_P010 &&
        in->mfx.FrameInfo.FourCC != MFX_FOURCC_P210
        )
        return false;

    // both zero or not zero
    if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        return false;

    if (in->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN /*&&
        in->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN10 &&
        in->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAINSP &&
        in->mfx.CodecProfile != MFX_PROFILE_MPEG2_REXT*/
        )
        return false;

    //BitDepthLuma & BitDepthChroma is also checked here
    if (!CheckFourcc(in->mfx.FrameInfo.FourCC, in->mfx.CodecProfile, &in->mfx.FrameInfo))
        return false;

    if (   in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || in->mfx.FrameInfo.FourCC == MFX_FOURCC_P210
        )
    {
        if (in->mfx.FrameInfo.Shift > 1)
            return false;
    }
    else
    {
        if (in->mfx.FrameInfo.Shift)
            return false;
    }

    switch (in->mfx.FrameInfo.PicStruct)
    {
    case MFX_PICSTRUCT_UNKNOWN:
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
    case MFX_PICSTRUCT_FIELD_REPEATED:
    case MFX_PICSTRUCT_FRAME_DOUBLING:
    case MFX_PICSTRUCT_FRAME_TRIPLING:
    case MFX_PICSTRUCT_FIELD_SINGLE:
        break;
    default:
        return false;
    }

    if (in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400 &&
        in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422
        )
        return false;

    if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return false;

    return true;
}

} } // namespace UMC_MPEG2_DECODER

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
