/******************************************************************************\
Copyright (c) 2020, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "mfx_ext.h"

class MFXAllocator : public mfxFrameAllocator
{
public:
    MFXAllocator(void)
    {
        auto self = static_cast<mfxFrameAllocator*>(this);
        *self = mfxFrameAllocator();
    }
    virtual ~MFXAllocator(void) {  }
    
    virtual mfxStatus Init(mfxHDL hdl, mfxFrameAllocatorType type) 
    {
        mfxFrameAllocator* a = nullptr;
        mfxStatus sts = MFXMemory_CreateAllocator(hdl, type, &a);
        if (sts != MFX_ERR_NONE)
            return sts;

        pthis = a->pthis;
        Alloc = a->Alloc;
        Lock = a->Lock;
        Free = a->Free;
        Unlock = a->Unlock;
        GetHDL = a->GetHDL;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus CreateSurfaces(mfxFrameInfo* info, mfxU32 num_surfaces, mfxFrameSurface1** surfaces)
    {
        mfxFrameAllocator a = {};
        a.pthis = pthis;
        a.Alloc = Alloc;
        a.Lock  = Lock;
        a.Free  = Free;
        a.Unlock = Unlock;
        a.GetHDL = GetHDL;

        return MFXMemory_CreateSurfaces(&a, info, num_surfaces, surfaces);
    }

    virtual mfxStatus ReleaseSurfaces(mfxU32 num_surfaces, mfxFrameSurface1* surfaces)
    {
        return MFXMemory_ReleaseSurfaces(num_surfaces, surfaces);
    }

    virtual mfxStatus LockSurface(mfxFrameSurface1* surface, mfxU32 flags)
    {
        return MFXMemory_LockSurface(surface, flags);
    }

    virtual mfxStatus UnlockSurface(mfxFrameSurface1* surface)
    {
        return MFXMemory_UnlockSurface(surface);
    }

    virtual mfxStatus GetHandle(mfxFrameSurface1* surface, mfxU32 type, mfxHDL* hdl)
    {
        return MFXMemory_GetHandle(surface, type, hdl);
    }

private:
    MFXAllocator(const MFXAllocator &);
    void operator=(MFXAllocator &);
};
