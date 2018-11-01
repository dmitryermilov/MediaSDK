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

#ifndef __UMC_MPEG2_HEADERS_H
#define __UMC_MPEG2_HEADERS_H

#include "umc_new_mpeg2_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_new_mpeg2_heap.h"

namespace UMC_MPEG2_DECODER
{

// Headers container class
template <typename T>
class HeaderSet
{
public:

    HeaderSet(Heap_Objects  *pObjHeap)
        : m_pObjHeap(pObjHeap)
        , m_currentID(-1)
    {
    }

    virtual ~HeaderSet()
    {
        Reset(false);
    }

    T * AddHeader(T* hdr)
    {
        uint32_t id = hdr->GetID();

        if (id >= m_Header.size())
        {
            m_Header.resize(id + 1);
        }

        m_currentID = id;

        if (m_Header[id])
        {
            m_Header[id]->DecrementReference();
        }

        T * header = m_pObjHeap->AllocateObject<T>();
        *header = *hdr;

        //ref. counter may not be 0 here since it can be copied from given [hdr] object
        header->ResetRefCounter();
        header->IncrementReference();

        m_Header[id] = header;
        return header;
    }

    T * GetHeader(int32_t id)
    {
        if ((uint32_t)id >= m_Header.size())
        {
            return 0;
        }

        return m_Header[id];
    }

    const T * GetHeader(int32_t id) const
    {
        if ((uint32_t)id >= m_Header.size())
        {
            return 0;
        }

        return m_Header[id];
    }

    void RemoveHeader(void * hdr)
    {
        T * tmp = (T *)hdr;
        if (!tmp)
        {
            VM_ASSERT(false);
            return;
        }

        uint32_t id = tmp->GetID();

        if (id >= m_Header.size())
        {
            VM_ASSERT(false);
            return;
        }

        if (!m_Header[id])
        {
            VM_ASSERT(false);
            return;
        }

        VM_ASSERT(m_Header[id] == hdr);
        m_Header[id]->DecrementReference();
        m_Header[id] = 0;
    }

    void Reset(bool isPartialReset = false)
    {
        if (!isPartialReset)
        {
            for (uint32_t i = 0; i < m_Header.size(); i++)
            {
                m_pObjHeap->FreeObject(m_Header[i]);
            }

            m_Header.clear();
            m_currentID = -1;
        }
    }

    void SetCurrentID(int32_t id)
    {
        if (GetHeader(id))
            m_currentID = id;
    }

    int32_t GetCurrentID() const
    {
        return m_currentID;
    }

    T * GetCurrentHeader()
    {
        if (m_currentID == -1)
            return 0;

        return GetHeader(m_currentID);
    }

    const T * GetCurrentHeader() const
    {
        if (m_currentID == -1)
            return 0;

        return GetHeader(m_currentID);
    }

private:
    std::vector<T*>           m_Header;
    Heap_Objects             *m_pObjHeap;

    int32_t                    m_currentID;
};

/****************************************************************************************************/
// Headers stuff
/****************************************************************************************************/
class Headers
{
public:

    Headers(Heap_Objects  *pObjHeap)
        : m_VideoParams(pObjHeap)
        , m_SeqParams(pObjHeap)
        , m_PicParams(pObjHeap)
        , m_SEIParams(pObjHeap)
        , m_SequenceParam(pObjHeap)
        , m_SequenceParamExt(pObjHeap)
        , m_SequenceDisplayExt(pObjHeap)
        , m_PictureParam(pObjHeap)
        , m_PictureParamExt(pObjHeap)
        , m_QuantMatrix(pObjHeap)
        , m_pObjHeap(pObjHeap)
    {
    }

    void Reset(bool isPartialReset = false)
    {
        m_SeqParams.Reset(isPartialReset);
        m_PicParams.Reset(isPartialReset);
        m_SEIParams.Reset(isPartialReset);
        m_VideoParams.Reset(isPartialReset);

        m_SequenceParam.Reset(isPartialReset);
        m_SequenceParamExt.Reset(isPartialReset);
        m_SequenceDisplayExt.Reset(isPartialReset);
        m_PictureParam.Reset(isPartialReset);
        m_PictureParamExt.Reset(isPartialReset);
        m_QuantMatrix.Reset(isPartialReset);
    }

    HeaderSet<MPEG2VideoParamSet>           m_VideoParams;
    HeaderSet<MPEG2SeqParamSet>             m_SeqParams;
    HeaderSet<MPEG2PicParamSet>             m_PicParams;
    HeaderSet<MPEG2SEIPayLoad>              m_SEIParams;

    HeaderSet<MPEG2SequenceHeader>            m_SequenceParam;
    HeaderSet<MPEG2SequenceExtension>         m_SequenceParamExt;
    HeaderSet<MPEG2SequenceDisplayExtension>  m_SequenceDisplayExt;
    HeaderSet<MPEG2PictureHeader>             m_PictureParam;
    HeaderSet<MPEG2PictureHeaderExtension>    m_PictureParamExt;
    HeaderSet<MPEG2QuantMatrix>               m_QuantMatrix;
private:
    Heap_Objects  *m_pObjHeap;
};

} // namespace UMC_MPEG2_DECODER

#endif // __UMC_MPEG2_HEADERS_H
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
