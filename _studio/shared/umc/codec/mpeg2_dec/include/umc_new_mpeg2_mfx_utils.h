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

#ifndef __UMC_MPEG2_MFX_UTILS_H
#define __UMC_MPEG2_MFX_UTILS_H

namespace UMC_MPEG2_DECODER
{

// MFX utility API functions
namespace MFX_Utility
{
    // Initialize mfxVideoParam structure based on decoded bitstream header values
    UMC::Status FillVideoParam(const MPEG2SeqParamSet * seq, mfxVideoParam *par, bool full);

    // Returns implementation platform
    eMFXPlatform GetPlatform_MPEG2(VideoCORE * core, mfxVideoParam * par);

    // Find bitstream header NAL units, parse them and fill application parameters structure
    UMC::Status DecodeHeader(TaskSupplier_MPEG2 * supplier, UMC::VideoDecoderParams* params, mfxBitstream *bs, mfxVideoParam *out);

    // MediaSDK DECODE_Query API function
    mfxStatus Query_MPEG2(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    // Validate input parameters
    bool CheckVideoParam_MPEG2(mfxVideoParam *in, eMFXHWType type);

    bool IsBugSurfacePoolApplicable(eMFXHWType hwtype, mfxVideoParam * par);

    // Check HW capabilities
    bool IsNeedPartialAcceleration_MPEG2(mfxVideoParam * par, eMFXHWType type);
};

} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_MFX_UTILS_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
