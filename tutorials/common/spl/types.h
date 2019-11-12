//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2019 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <string.h>
#include <memory>
#include "mfxstructures.h"

// class is used as custom exception
class mfxError : public std::runtime_error
{
public:
    mfxError(mfxStatus status = MFX_ERR_UNKNOWN, std::string msg = "")
        : runtime_error(msg)
        , m_Status(status)
    {}

    mfxStatus GetStatus() const
    { return m_Status; }

private:
    mfxStatus m_Status;
};

//declare used extension buffers
template<class T>
struct mfx_ext_buffer_id{};

template<>struct mfx_ext_buffer_id<mfxExtCodingOption>{
    enum {id = MFX_EXTBUFF_CODING_OPTION};
};
template<>struct mfx_ext_buffer_id<mfxExtCodingOption2>{
    enum {id = MFX_EXTBUFF_CODING_OPTION2};
};
template<>struct mfx_ext_buffer_id<mfxExtCodingOption3>{
    enum {id = MFX_EXTBUFF_CODING_OPTION3};
};
template<>struct mfx_ext_buffer_id<mfxExtThreadsParam>{
    enum {id = MFX_EXTBUFF_THREADS_PARAM};
};

constexpr uint16_t max_num_ext_buffers = 63 * 2; // '*2' is for max estimation if all extBuffer were 'paired'

//helper function to initialize mfx ext buffer structure
template <class T>
void init_ext_buffer(T & ext_buffer)
{
    memset(&ext_buffer, 0, sizeof(ext_buffer));
    reinterpret_cast<mfxExtBuffer*>(&ext_buffer)->BufferId = mfx_ext_buffer_id<T>::id;
    reinterpret_cast<mfxExtBuffer*>(&ext_buffer)->BufferSz = sizeof(ext_buffer);
}

template <typename T> struct IsPairedMfxExtBuffer                 : std::false_type {};
template <> struct IsPairedMfxExtBuffer<mfxExtAVCRefListCtrl>     : std::true_type {};
template <> struct IsPairedMfxExtBuffer<mfxExtAVCRoundingOffset>  : std::true_type {};
template <> struct IsPairedMfxExtBuffer<mfxExtPredWeightTable>    : std::true_type {};

template <typename R>
struct ExtParamAccessor
{
private:
    using mfxExtBufferDoublePtr = mfxExtBuffer**;
public:
    mfxU16& NumExtParam;
    mfxExtBufferDoublePtr& ExtParam;
    ExtParamAccessor(const R& r):
        NumExtParam(const_cast<mfxU16&>(r.NumExtParam)),
        ExtParam(const_cast<mfxExtBufferDoublePtr&>(r.ExtParam)) {}
};

template <>
struct ExtParamAccessor<mfxFrameSurface1>
{
private:
    using mfxExtBufferDoublePtr = mfxExtBuffer**;
public:
    mfxU16& NumExtParam;
    mfxExtBufferDoublePtr& ExtParam;
    ExtParamAccessor(const mfxFrameSurface1& r):
        NumExtParam(const_cast<mfxU16&>(r.Data.NumExtParam)),
        ExtParam(const_cast<mfxExtBufferDoublePtr&>(r.Data.ExtParam)) {}
};

/** ExtBufHolder is an utility class which
 *  provide interface for mfxExtBuffer objects management in any mfx structure (e.g. mfxVideoParam)
 */
template<typename T>
class ExtBufHolder : public T
{
public:
    ExtBufHolder() : T()
    {
        m_ext_buf.reserve(max_num_ext_buffers);
    }

    ~ExtBufHolder() // only buffers allocated by wrapper can be released
    {
        for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++ )
        {
            delete [] (mfxU8*)(*it);
        }
    }

    ExtBufHolder(const ExtBufHolder& ref)
    {
        m_ext_buf.reserve(max_num_ext_buffers);
        *this = ref; // call to operator=
    }

    ExtBufHolder& operator=(const ExtBufHolder& ref)
    {
        const T* src_base = &ref;
        return operator=(*src_base);
    }

    ExtBufHolder(const T& ref)
    {
        *this = ref; // call to operator=
    }

    ExtBufHolder& operator=(const T& ref)
    {
        // copy content of main structure type T
        T* dst_base = this;
        const T* src_base = &ref;
        *dst_base = *src_base;

        //remove all existing extension buffers
        ClearBuffers();

        const auto ref_ = ExtParamAccessor<T>(ref);

        //reproduce list of extension buffers and copy its content
        for (size_t i = 0; i < ref_.NumExtParam; ++i)
        {
            const auto src_buf = ref_.ExtParam[i];
            if (!src_buf) throw mfxError(MFX_ERR_NULL_PTR, "Null pointer attached to source ExtParam");
            if (!IsCopyAllowed(src_buf->BufferId))
            {
                auto msg = "Deep copy of '" + Fourcc2Str(src_buf->BufferId) + "' extBuffer is not allowed";
                throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, msg);
            }

            // 'false' below is because here we just copy extBuffer's one by one
            auto dst_buf = AddExtBuffer(src_buf->BufferId, src_buf->BufferSz, false);
            // copy buffer content w/o restoring its type
            memcpy((void*)dst_buf, (void*)src_buf, src_buf->BufferSz);
        }

        return *this;
    }

    ExtBufHolder(ExtBufHolder &&)             = default;
    ExtBufHolder & operator= (ExtBufHolder&&) = default;

    // Always returns a valid pointer or throws an exception
    template<typename TB>
    TB* AddExtBuffer()
    {
        mfxExtBuffer* b = AddExtBuffer(mfx_ext_buffer_id<TB>::id, sizeof(TB), IsPairedMfxExtBuffer<TB>::value);
        return (TB*)b;
    }

    template<typename TB>
    void RemoveExtBuffer()
    {
        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(mfx_ext_buffer_id<TB>::id));
        if (it != m_ext_buf.end())
        {
            delete [] (mfxU8*)(*it);
            it = m_ext_buf.erase(it);

            if (IsPairedMfxExtBuffer<TB>::value)
            {
                if (it == m_ext_buf.end() || (*it)->BufferId != mfx_ext_buffer_id<TB>::id)
                    throw mfxError(MFX_ERR_NULL_PTR, "RemoveExtBuffer: ExtBuffer's parity has been broken");

                delete [] (mfxU8*)(*it);
                m_ext_buf.erase(it);
            }

            RefreshBuffers();
        }
    }

    template <typename TB>
    TB* GetExtBuffer(uint32_t fieldId = 0) const
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, fieldId);
    }

    template <typename TB>
    operator TB*()
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
    }

    template <typename TB>
    operator TB*() const
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
    }

private:

    mfxExtBuffer* AddExtBuffer(mfxU32 id, mfxU32 size, bool isPairedExtBuffer)
    {
        if (!size || !id)
            throw mfxError(MFX_ERR_NULL_PTR, "AddExtBuffer: wrong size or id!");

        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
        if (it == m_ext_buf.end())
        {
            auto buf = (mfxExtBuffer*)new mfxU8[size];
            memset(buf, 0, size);
            m_ext_buf.push_back(buf);

            buf->BufferId = id;
            buf->BufferSz = size;

            if (isPairedExtBuffer)
            {
                // Allocate the other mfxExtBuffer _right_after_ the first one ...
                buf = (mfxExtBuffer*)new mfxU8[size];
                memset(buf, 0, size);
                m_ext_buf.push_back(buf);

                buf->BufferId = id;
                buf->BufferSz = size;

                RefreshBuffers();
                return m_ext_buf[m_ext_buf.size() - 2]; // ... and return a pointer to the first one
            }

            RefreshBuffers();
            return m_ext_buf.back();
        }

        return *it;
    }

    mfxExtBuffer* FindExtBuffer(mfxU32 id, uint32_t fieldId) const
    {
        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
        if (fieldId && it != m_ext_buf.end())
        {
            ++it;
            return it != m_ext_buf.end() ? *it : nullptr;
        }
        return it != m_ext_buf.end() ? *it : nullptr;
    }

    void RefreshBuffers()
    {
        auto this_ = ExtParamAccessor<T>(*this);
        this_.NumExtParam = static_cast<mfxU16>(m_ext_buf.size());
        this_.ExtParam    = this_.NumExtParam ? m_ext_buf.data() : nullptr;
    }

    void ClearBuffers()
    {
        if (m_ext_buf.size())
        {
            for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++ )
            {
                delete [] (mfxU8*)(*it);
            }
            m_ext_buf.clear();
        }
        RefreshBuffers();
    }

    bool IsCopyAllowed(mfxU32 id)
    {
        static const mfxU32 allowed[] = {
            MFX_EXTBUFF_CODING_OPTION,
            MFX_EXTBUFF_CODING_OPTION2,
            MFX_EXTBUFF_CODING_OPTION3,
        };

        auto it = std::find_if(std::begin(allowed), std::end(allowed),
                               [&id](const mfxU32 allowed_id)
                               {
                                   return allowed_id == id;
                               });
        return it != std::end(allowed);
    }

    struct CmpExtBufById
    {
        mfxU32 id;

        CmpExtBufById(mfxU32 _id)
            : id(_id)
        { };

        bool operator () (mfxExtBuffer* b)
        {
            return  (b && b->BufferId == id);
        };
    };

    static std::string Fourcc2Str(mfxU32 fourcc)
    {
        std::string s;
        for (size_t i = 0; i < 4; i++)
        {
            s.push_back(*(i + (char*)&fourcc));
        }
        return s;
    }

    std::vector<mfxExtBuffer*> m_ext_buf;
};

class mfxBitstreamWrapper : public ExtBufHolder<mfxBitstream>
{
    typedef ExtBufHolder<mfxBitstream> base;
public:
    mfxBitstreamWrapper()
        : base()
    {}

    mfxBitstreamWrapper(mfxU32 n_bytes)
        : base()
    {
        Extend(n_bytes);
    }

    mfxBitstreamWrapper(const mfxBitstreamWrapper & bs_wrapper)
        : base(bs_wrapper)
        , m_data(bs_wrapper.m_data)
    {
        Data = m_data.data();
    }

    mfxBitstreamWrapper& operator=(mfxBitstreamWrapper const& bs_wrapper)
    {
        mfxBitstreamWrapper tmp(bs_wrapper);

        *this = std::move(tmp);

        return *this;
    }

    mfxBitstreamWrapper(mfxBitstreamWrapper && bs_wrapper)             = default;
    mfxBitstreamWrapper & operator= (mfxBitstreamWrapper&& bs_wrapper) = default;
    ~mfxBitstreamWrapper()                                             = default;

    void Extend(mfxU32 n_bytes)
    {
        if (MaxLength >= n_bytes)
            return;

        m_data.resize(n_bytes);

        Data      = m_data.data();
        MaxLength = n_bytes;
    }
private:
    std::vector<mfxU8> m_data;
};
