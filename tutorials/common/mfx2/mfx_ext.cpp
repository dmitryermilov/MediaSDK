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

#include <vector>
#include "mfxsession.h"
#include "mfxvideo.h"
#include "mfx_ext.h"

#include "default_vaapi_allocator.h"

mfxStatus MFXMemory_CreateAllocator(mfxHDL hdl, mfxFrameAllocatorType type, mfxFrameAllocator** allocator)
{
    if (!hdl)
        return MFX_ERR_NULL_PTR;

    if (!allocator)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = MFX_ERR_NONE;

    if (MFX_FRAME_ALLOCATOR_VAAPI == type)
    {
        auto alloc = new VAAPIFrameAllocator(hdl);

        sts = alloc->Init();
        if (sts != MFX_ERR_NONE)
            return sts;

        *allocator = alloc;
    }
    else if (MFX_FRAME_ALLOCATOR_D3D9 == type)
    {
        // TODO
    }
    else if (MFX_FRAME_ALLOCATOR_D3D11 == type)
    {
        // TODO
    }

    return MFX_ERR_NONE;
}

struct mfxFrameSurfacePrivCtx
{
    mfxFrameAllocator allocator;
};

mfxStatus MFXMemory_CreateSurfaces(mfxFrameAllocator* allocator, mfxFrameInfo* info, mfxU32 num_surfaces, mfxFrameSurface1** surfaces)
{
    if (!allocator)
        return MFX_ERR_NULL_PTR;

    if (!info)
        return MFX_ERR_NULL_PTR;

    if (!surfaces)
        return MFX_ERR_NULL_PTR;

    mfxFrameSurface1* s = new mfxFrameSurface1[num_surfaces]();

    mfxFrameAllocRequest req = {};
    req.Info = *info;
    req.NumFrameMin = req.NumFrameSuggested = 1;

    mfxMemId mid = {};

    mfxFrameAllocResponse response = {};
    response.mids = &mid;

    for (mfxU32 i = 0; i < num_surfaces; ++i)
    {
        auto sts = allocator->Alloc(allocator->pthis, &req, &response);
        if (sts != MFX_ERR_NONE)
            return sts;

        auto privCtx = new mfxFrameSurfacePrivCtx();
        privCtx->allocator = *allocator;
        s[i].pthis = (mfxHDL)privCtx;
        s[i].Info = *info;
        s[i].Data.MemId = response.mids[0];
    }

    *surfaces = s;

    return MFX_ERR_NONE;
}

mfxStatus MFXMemory_ReleaseSurfaces(mfxU32 num_surfaces, mfxFrameSurface1* surfaces)
{
    if (!surfaces)
        return MFX_ERR_NULL_PTR;

    mfxMemId mid = {};

    mfxFrameAllocResponse response = {};
    response.NumFrameActual = 1;
    response.mids = &mid;
    for (mfxU32 i = 0; i < num_surfaces; ++i)
    {
        auto privCtx = (mfxFrameSurfacePrivCtx*)surfaces[i].pthis;
        response.mids[0] = surfaces[i].Data.MemId;
        auto sts = privCtx->allocator.Free(privCtx->allocator.pthis, &response);
        if (sts != MFX_ERR_NONE)
            return sts;

        delete privCtx;
    }

    delete[] surfaces;

    return MFX_ERR_NONE;
}

mfxStatus MFXMemory_LockSurface(mfxFrameSurface1* surface, mfxU32 flags)
{
    if (!surface)
        return MFX_ERR_NULL_PTR;

    auto privCtx = (mfxFrameSurfacePrivCtx*)surface->pthis;

    return privCtx->allocator.Lock(privCtx->allocator.pthis, surface->Data.MemId, &surface->Data);
}

mfxStatus MFXMemory_UnlockSurface(mfxFrameSurface1* surface)
{
    if (!surface)
        return MFX_ERR_NULL_PTR;

    auto privCtx = (mfxFrameSurfacePrivCtx*)surface->pthis;

    return privCtx->allocator.Unlock(privCtx->allocator.pthis, surface->Data.MemId, &surface->Data);
}

mfxStatus MFXMemory_GetHandle(mfxFrameSurface1* surface, mfxU32 /*type*/, mfxHDL* hdl)
{
    if (!surface)
        return MFX_ERR_NULL_PTR;

    auto privCtx = (mfxFrameSurfacePrivCtx*)surface->pthis;

    return privCtx->allocator.GetHDL(privCtx->allocator.pthis, surface->Data.MemId, hdl);
}
