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

#include "mfxstructures.h"
#include "types.h"
#include "abstract_splitter.h"

class CSmplBitstreamReader
{
public :

    CSmplBitstreamReader();
    virtual ~CSmplBitstreamReader();

    //resets position to file begin
    virtual void      Reset();
    virtual void      Close();
    virtual mfxStatus Init(const std::string & strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);

protected:
    FILE*     m_fSource;
    bool      m_bInited;
};

class AbstractSplitter;
class CSplitterBitstreamReader : public CSmplBitstreamReader
{
public:
    CSplitterBitstreamReader(mfxU32 codecID);
    virtual ~CSplitterBitstreamReader();

    /** Free resources.*/
    virtual void      Close();
    virtual mfxStatus Init(const std::string & strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);

private:
    mfxBitstream *m_processedBS;
    // input bit stream
    mfxBitstreamWrapper m_originalBS;

    mfxStatus PrepareNextFrame(mfxBitstream *in, mfxBitstream **out);

    mfxU32 m_codecID;
    // is stream ended
    bool m_isEndOfStream;

    std::unique_ptr<AbstractSplitter> m_pNALSplitter;
    FrameSplitterInfo *m_frame;
    mfxU8 *m_plainBuffer;
    mfxU32 m_plainBufferSize;
    mfxBitstream m_outBS;
};
