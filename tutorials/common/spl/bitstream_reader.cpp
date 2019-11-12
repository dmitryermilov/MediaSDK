/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2019 Intel Corporation. All Rights Reserved.
//
//
*/

#include "common_utils.h"
#include "bitstream_reader.h"
#include "avc_spl.h"

CSmplBitstreamReader::CSmplBitstreamReader()
{
    m_fSource = NULL;
    m_bInited = false;
}

CSmplBitstreamReader::~CSmplBitstreamReader()
{
    Close();
}

void CSmplBitstreamReader::Close()
{
    if (m_fSource)
    {
        fclose(m_fSource);
        m_fSource = NULL;
    }

    m_bInited = false;
}

void CSmplBitstreamReader::Reset()
{
    if (!m_bInited)
        return;

    fseek(m_fSource, 0, SEEK_SET);
}

mfxStatus CSmplBitstreamReader::Init(const std::string & strFileName)
{
    Close();

    //open file to read input stream
    MSDK_FOPEN(m_fSource, strFileName.c_str(), "rb");
    MSDK_CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

    m_bInited = true;
    return MFX_ERR_NONE;
}

#define CHECK_SET_EOS(pBitstream)                  \
    if (feof(m_fSource))                           \
    {                                              \
        pBitstream->DataFlag |= MFX_BITSTREAM_EOS; \
    }

mfxStatus CSmplBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    if (!m_bInited)
        return MFX_ERR_NOT_INITIALIZED;

    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);

    // Not enough memory to read new chunk of data
    if (pBS->MaxLength == pBS->DataLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;
    mfxU32 nBytesRead = (mfxU32)fread(pBS->Data + pBS->DataLength, 1, pBS->MaxLength - pBS->DataLength, m_fSource);

    CHECK_SET_EOS(pBS);

    if (0 == nBytesRead)
    {
        return MFX_ERR_MORE_DATA;
    }

    pBS->DataLength += nBytesRead;

    return MFX_ERR_NONE;
}


CSplitterBitstreamReader::CSplitterBitstreamReader(mfxU32 codecID)
  : CSmplBitstreamReader()
  , m_processedBS(0)
  , m_codecID(codecID)
  , m_isEndOfStream(false)
  , m_frame(0)
  , m_plainBuffer(0)
  , m_plainBufferSize(0)
{
}

CSplitterBitstreamReader::~CSplitterBitstreamReader()
{
}

void CSplitterBitstreamReader::Close()
{
    CSmplBitstreamReader::Close();

    if (NULL != m_plainBuffer)
    {
        free(m_plainBuffer);
        m_plainBuffer = NULL;
        m_plainBufferSize = 0;
    }
}

mfxStatus CSplitterBitstreamReader::Init(const std::string & strFileName)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CSmplBitstreamReader::Init(strFileName);
    if (sts != MFX_ERR_NONE)
        return sts;

    m_isEndOfStream = false;
    m_processedBS = NULL;

    m_originalBS.Extend(1024 * 1024);

    if (MFX_CODEC_AVC == m_codecID)
      m_pNALSplitter.reset(new ProtectedLibrary::AVC_Spl());
    else if (MFX_CODEC_HEVC == m_codecID)
        m_pNALSplitter.reset(0);

    m_frame = 0;
    m_plainBuffer = 0;
    m_plainBufferSize = 0;

    return sts;
}

mfxStatus CopyBitstream2(mfxBitstream *dest, mfxBitstream *src)
{
    if (!dest || !src)
        return MFX_ERR_NULL_PTR;

    if (!dest->DataLength)
    {
        dest->DataOffset = 0;
    }
    else
    {
        memmove(dest->Data, dest->Data + dest->DataOffset, dest->DataLength);
        dest->DataOffset = 0;
    }

    if (src->DataLength > dest->MaxLength - dest->DataLength - dest->DataOffset)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    memcpy(dest->Data + dest->DataOffset, src->Data, src->DataLength);
    dest->DataLength = src->DataLength;

    dest->DataFlag = src->DataFlag;

    //common Extended buffer will be for src and dest bit streams
    dest->EncryptedData = src->EncryptedData;

    return MFX_ERR_NONE;
}

mfxStatus CSplitterBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    mfxStatus sts = MFX_ERR_NONE;
    pBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    //read bit stream from source
    while (!m_originalBS.DataLength)
    {
        sts = CSmplBitstreamReader::ReadNextFrame(&m_originalBS);
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA)
            return sts;
        if (sts == MFX_ERR_MORE_DATA)
        {
            m_isEndOfStream = true;
            break;
        }
    }

    do
    {
        sts = PrepareNextFrame(m_isEndOfStream ? NULL : &m_originalBS, &m_processedBS);

        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_isEndOfStream)
            {
                break;
            }

            sts = CSmplBitstreamReader::ReadNextFrame(&m_originalBS);
            if (sts == MFX_ERR_MORE_DATA)
                m_isEndOfStream = true;
            continue;
        }
        else if (MFX_ERR_NONE != sts)
            return sts;

    } while (MFX_ERR_NONE != sts);

    // get output stream
    if (NULL != m_processedBS)
    {
        mfxStatus copySts = CopyBitstream2(
            pBS,
            m_processedBS);
        if (copySts < MFX_ERR_NONE)
            return copySts;
        m_processedBS = NULL;
    }

    return sts;
}

mfxStatus CSplitterBitstreamReader::PrepareNextFrame(mfxBitstream *in, mfxBitstream **out)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (NULL == out)
        return MFX_ERR_NULL_PTR;

    *out = NULL;

    // get frame if it is not ready yet
    if (NULL == m_frame)
    {
        sts = m_pNALSplitter->GetFrame(in, &m_frame);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    if (m_plainBufferSize < m_frame->DataLength)
    {
        if (NULL != m_plainBuffer)
        {
            free(m_plainBuffer);
            m_plainBuffer = NULL;
            m_plainBufferSize = 0;
        }
        m_plainBuffer = (mfxU8*)malloc(m_frame->DataLength);
        if (NULL == m_plainBuffer)
            return MFX_ERR_MEMORY_ALLOC;
        m_plainBufferSize = m_frame->DataLength;
    }

    memcpy(m_plainBuffer, m_frame->Data, m_frame->DataLength);

    memset(&m_outBS, 0, sizeof(mfxBitstream));
    m_outBS.Data = m_plainBuffer;
    m_outBS.DataOffset = 0;
    m_outBS.DataLength = m_frame->DataLength;
    m_outBS.MaxLength = m_frame->DataLength;
    m_outBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    m_outBS.TimeStamp = m_frame->TimeStamp;

    m_pNALSplitter->ResetCurrentState();
    m_frame = NULL;

    *out = &m_outBS;

    return sts;
}
