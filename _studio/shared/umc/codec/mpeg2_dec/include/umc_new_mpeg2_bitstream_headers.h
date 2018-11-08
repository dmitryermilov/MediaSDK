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

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_MPEG2_VIDEO_DECODER

#ifndef __UMC_MPEG2_BITSTREAM_HEADERS_H_
#define __UMC_MPEG2_BITSTREAM_HEADERS_H_

#include "umc_structures.h"
#include "umc_new_mpeg2_dec_defs.h"

// Read N bits from 32-bit array
#define GetNBits(current_data, offset, nbits, data) \
{ \
    uint32_t x; \
 \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset -= (nbits); \
 \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
 \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
        current_data++; \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

// Return bitstream position pointers N bits back
#define UngetNBits(current_data, offset, nbits) \
{ \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset += (nbits); \
    if (offset > 31) \
    { \
        offset -= 32; \
        current_data--; \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
}

// Skip N bits in 32-bit array
#define SkipNBits(current_data, offset, nbits) \
{ \
    /* check error(s) */ \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(offset >= 0 && offset <= 31); \
    /* decrease number of available bits */ \
    offset -= (nbits); \
    /* normalize bitstream pointer */ \
    if (0 > offset) \
    { \
        offset += 32; \
        current_data++; \
    } \
    /* check error(s) again */ \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 }

// Read 1 bit from 32-bit array
#define GetBits1(current_data, offset, data) \
{ \
    data = ((current_data[0] >> (offset)) & 1);  \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

// Check 1 bit from 32-bit array
#define CheckBit1(current_data, offset, data) \
{ \
    data = ((current_data[0] >> (offset)) & 1);  \
}

// Align bitstream position to byte boundary
#define ippiAlignBSPointerRight(current_data, offset) \
{ \
    if ((offset & 0x07) != 0x07) \
    { \
        offset = (offset | 0x07) - 8; \
        if (offset == -1) \
        { \
            offset = 31; \
            current_data++; \
        } \
    } \
}

// Read N bits from 32-bit array
#define PeakNextBits(current_data, bp, nbits, data) \
{ \
    uint32_t x; \
 \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(nbits >= 0 && nbits <= 31); \
 \
    int32_t offset = bp - (nbits); \
 \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
 \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

namespace UMC_MPEG2_DECODER
{
// Bit masks for fast extraction of bits from bitstream
const uint32_t bits_data[33] =
{
    (((uint32_t)0x01 << (0)) - 1),
    (((uint32_t)0x01 << (1)) - 1),
    (((uint32_t)0x01 << (2)) - 1),
    (((uint32_t)0x01 << (3)) - 1),
    (((uint32_t)0x01 << (4)) - 1),
    (((uint32_t)0x01 << (5)) - 1),
    (((uint32_t)0x01 << (6)) - 1),
    (((uint32_t)0x01 << (7)) - 1),
    (((uint32_t)0x01 << (8)) - 1),
    (((uint32_t)0x01 << (9)) - 1),
    (((uint32_t)0x01 << (10)) - 1),
    (((uint32_t)0x01 << (11)) - 1),
    (((uint32_t)0x01 << (12)) - 1),
    (((uint32_t)0x01 << (13)) - 1),
    (((uint32_t)0x01 << (14)) - 1),
    (((uint32_t)0x01 << (15)) - 1),
    (((uint32_t)0x01 << (16)) - 1),
    (((uint32_t)0x01 << (17)) - 1),
    (((uint32_t)0x01 << (18)) - 1),
    (((uint32_t)0x01 << (19)) - 1),
    (((uint32_t)0x01 << (20)) - 1),
    (((uint32_t)0x01 << (21)) - 1),
    (((uint32_t)0x01 << (22)) - 1),
    (((uint32_t)0x01 << (23)) - 1),
    (((uint32_t)0x01 << (24)) - 1),
    (((uint32_t)0x01 << (25)) - 1),
    (((uint32_t)0x01 << (26)) - 1),
    (((uint32_t)0x01 << (27)) - 1),
    (((uint32_t)0x01 << (28)) - 1),
    (((uint32_t)0x01 << (29)) - 1),
    (((uint32_t)0x01 << (30)) - 1),
    (((uint32_t)0x01 << (31)) - 1),
    ((uint32_t)0xFFFFFFFF),
};


template <typename T> class HeaderSet;
class Headers;

// Bitstream low level parsing class
class MPEG2BaseBitstream
{
public:

    MPEG2BaseBitstream();
    MPEG2BaseBitstream(uint8_t * const pb, const uint32_t maxsize);
    virtual ~MPEG2BaseBitstream();

    // Reset the bitstream with new data pointer
    void Reset(uint8_t * const pb, const uint32_t maxsize);
    // Reset the bitstream with new data pointer and bit offset
    void Reset(uint8_t * const pb, int32_t offset, const uint32_t maxsize);

    // Align bitstream position to byte boundary
    inline void AlignPointerRight(void);

    // Read N bits from bitstream array
    inline uint32_t GetBits(uint32_t nbits);

    // Return bitstream position pointers N bits back
    inline void UngetBits(uint32_t nbits);

    // Read N bits from bitstream array
    template <uint32_t nbits>
    inline uint32_t GetPredefinedBits();

    // Read variable length coded unsigned element
    uint32_t GetVLCElementU();

    // Read variable length coded signed element
    int32_t GetVLCElementS();

    // Reads one bit from the buffer.
    uint8_t Get1Bit();

    // Check next bit in the buffer.
    uint8_t Check1Bit();

    // Check that position in bitstream didn't move outside the limit
    bool CheckBSLeft();

    // Check whether more data is present
    bool More_RBSP_Data();

    // Returns number of decoded bytes since last reset
    size_t BytesDecoded() const;

    // Returns number of decoded bits since last reset
    size_t BitsDecoded() const;

    // Returns number of bytes left in bitstream array
    size_t BytesLeft() const;

    // Returns number of bits needed for byte alignment
    unsigned getNumBitsUntilByteAligned() const;

    // Align bitstream to byte boundary
    void readOutTrailingBits();

    // Returns bitstream current buffer pointer
    const uint8_t *GetRawDataPtr() const    {
        return (const uint8_t *)m_pbs + (31 - m_bitOffset)/8;
    }

    // Return bitstream array base address and size
    void GetOrg(uint32_t **pbs, uint32_t *size);
    // Return current bitstream address and bit offset
    void GetState(uint32_t **pbs, uint32_t *bitOffset);
    // Set current bitstream address and bit offset
    void SetState(uint32_t *pbs, uint32_t bitOffset);

    // Set current decoding position
    void SetDecodedBytes(size_t);

    size_t GetAllBitsCount()
    {
        return m_maxBsSize;
    }

    size_t BytesDecodedRoundOff()
    {
        return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase);
    }

protected:

    uint32_t *m_pbs;                                              // (uint32_t *) pointer to the current position of the buffer.
    int32_t m_bitOffset;                                         // (int32_t) the bit position (0 to 31) in the dword pointed by m_pbs.
    uint32_t *m_pbsBase;                                          // (uint32_t *) pointer to the first byte of the buffer.
    uint32_t m_maxBsSize;                                         // (uint32_t) maximum buffer size in bytes.
};

class MPEG2ScalingList;
class MPEG2VideoParamSet;
struct MPEG2SeqParamSet;
class MPEG2Slice;

// Bitstream headers parsing class
class MPEG2HeadersBitstream : public MPEG2BaseBitstream
{
public:

    MPEG2HeadersBitstream();
    MPEG2HeadersBitstream(uint8_t * const pb, const uint32_t maxsize);

    // Read and return NAL unit type and NAL storage idc.
    // Bitstream position is expected to be at the start of a NAL unit.
    UMC::Status GetNALUnitType(NalUnitType &nal_unit_type, uint32_t &nuh_temporal_id);

    // Read and return a header type.
    // Bitstream position is expected to be at the start of a NAL unit.
    UMC::Status GetNALUnitType(NalUnitType &nal_unit_type);

    // Read optional access unit delimiter from bitstream.
    UMC::Status GetAccessUnitDelimiter(uint32_t &PicCodType);

    // Parse SEI message
    int32_t ParseSEI(const HeaderSet<MPEG2SeqParamSet> & sps, int32_t current_sps, MPEG2SEIPayLoad *spl);

    // Parse remaining of slice header after GetSliceHeaderPart1
    void decodeSlice(MPEG2Slice *, const MPEG2SeqParamSet *, const MPEG2PicParamSet *);
    // Parse slice header part which contains PPS ID
    UMC::Status GetSliceHeaderPart1(MPEG2SliceHeader * sliceHdr);
    UMC::Status GetSliceHeader(MPEG2SliceHeader_ * sliceHdr, const MPEG2SequenceHeader *, const MPEG2SequenceExtension *);
    // Decode macroblock_address_increment (MBA)
    UMC::Status DecodeMBAddress(MPEG2SliceHeader_ * sliceHdr);
    // Parse full slice header
    UMC::Status GetSliceHeaderFull(MPEG2Slice *, const MPEG2SequenceHeader *, const MPEG2SequenceExtension *);

    // Parse scaling list information in SPS or PPS
    void parseScalingList(MPEG2ScalingList *);
    // Reserved for future header extensions
    bool MoreRbspData();

/*********************************MPEG2******************************************/
    // Parse sequence header
    void GetSequenceHeader(MPEG2SequenceHeader *seq);

    // Parse sequence extension
    void GetSequenceExtension(MPEG2SequenceExtension *seqExt);

    // Parse sequence display extension
    void GetSequenceDisplayExtension(MPEG2SequenceDisplayExtension *dispExt);

    // Parse picture header
    void GetPictureHeader(MPEG2PictureHeader *pic);

    // Parse picture extension
    void GetPictureExtensionHeader(MPEG2PictureCodingExtension *picExt);

    // Parse quant matrix extension
    void GetQuantMatrix(MPEG2QuantMatrix *q);
/*********************************MPEG2******************************************/
    // Part VPS header
    UMC::Status GetVideoParamSet(MPEG2VideoParamSet *vps);

    // Parse SPS header
    UMC::Status GetSequenceParamSet(MPEG2SeqParamSet *sps);

    // Parse PPS header
    void GetPictureParamSetPart1(MPEG2PicParamSet *pps);
    UMC::Status GetPictureParamSetFull(MPEG2PicParamSet  *pps, MPEG2SeqParamSet const*);
    UMC::Status GetWPPTileInfo(MPEG2SliceHeader *hdr,
                            const MPEG2PicParamSet *pps,
                            const MPEG2SeqParamSet *sps);

    void parseShortTermRefPicSet(const MPEG2SeqParamSet* sps, ReferencePictureSet* pRPS, uint32_t idx);

protected:

    // Parse video usability information block in SPS
    void parseVUI(MPEG2SeqParamSet *sps);

    // Parse weighted prediction table in slice header
    void xParsePredWeightTable(const MPEG2SeqParamSet *sps, MPEG2SliceHeader * sliceHdr);
    // Parse scaling list data block
    void xDecodeScalingList(MPEG2ScalingList *scalingList, unsigned sizeId, unsigned listId);
    // Parse HRD information in VPS or in VUI block of SPS
    void parseHrdParameters(MPEG2HRD *hrd, uint8_t commonInfPresentFlag, uint32_t vps_max_sub_layers);

    // Parse profile tier layers header part in VPS or SPS
    void  parsePTL(MPEG2ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1);
    // Parse one profile tier layer
    void  parseProfileTier(MPEG2PTL *ptl);

    // Decoding SEI message functions
    int32_t sei_message(const HeaderSet<MPEG2SeqParamSet> & sps,int32_t current_sps,MPEG2SEIPayLoad *spl);
    // Parse SEI payload data
    int32_t sei_payload(const HeaderSet<MPEG2SeqParamSet> & sps, int32_t current_sps, MPEG2SEIPayLoad *spl);
    // Parse pic timing SEI data
    int32_t pic_timing(const HeaderSet<MPEG2SeqParamSet> & sps, int32_t current_sps, MPEG2SEIPayLoad *spl);
    // Parse recovery point SEI data
    int32_t recovery_point(const HeaderSet<MPEG2SeqParamSet> & sps, int32_t current_sps, MPEG2SEIPayLoad *spl);

    // Skip unrecognized SEI message payload
    int32_t reserved_sei_message(const HeaderSet<MPEG2SeqParamSet> & sps, int32_t current_sps, MPEG2SEIPayLoad *spl);
};


// Read N bits from bitstream array
inline
uint32_t MPEG2BaseBitstream::GetBits(const uint32_t nbits)
{
    uint32_t w, n = nbits;

    GetNBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

// Read N bits from bitstream array
inline
void MPEG2BaseBitstream::UngetBits(const uint32_t nbits)
{
    UngetNBits(m_pbs, m_bitOffset, nbits);
}

// Read N bits from bitstream array
template <uint32_t nbits>
inline uint32_t MPEG2BaseBitstream::GetPredefinedBits()
{
    uint32_t w, n = nbits;

    GetNBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

inline bool DecodeExpGolombOne_MPEG2_1u32s (uint32_t **ppBitStream,
                                                      int32_t *pBitOffset,
                                                      int32_t *pDst,
                                                      int32_t isSigned)
{
    uint32_t code;
    uint32_t info     = 0;
    int32_t length   = 1;            /* for first bit read above*/
    uint32_t thisChunksLength = 0;
    uint32_t sval;

    /* check error(s) */

    /* Fast check for element = 0 */
    GetNBits((*ppBitStream), (*pBitOffset), 1, code)
    if (code)
    {
        *pDst = 0;
        return true;
    }

    GetNBits((*ppBitStream), (*pBitOffset), 8, code);
    length += 8;

    /* find nonzero byte */
    while (code == 0 && 32 > length)
    {
        GetNBits((*ppBitStream), (*pBitOffset), 8, code);
        length += 8;
    }

    /* find leading '1' */
    while ((code & 0x80) == 0 && 32 > thisChunksLength)
    {
        code <<= 1;
        thisChunksLength++;
    }
    length -= 8 - thisChunksLength;

    UngetNBits((*ppBitStream), (*pBitOffset),8 - (thisChunksLength + 1))

    /* skipping very long codes, let's assume what the code is corrupted */
    if (32 <= length || 32 <= thisChunksLength)
    {
        uint32_t dwords;
        length -= (*pBitOffset + 1);
        dwords = length/32;
        length -= (32*dwords);
        *ppBitStream += (dwords + 1);
        *pBitOffset = 31 - length;
        *pDst = 0;
        return false;
    }

    /* Get info portion of codeword */
    if (length)
    {
        GetNBits((*ppBitStream), (*pBitOffset),length, info)
    }

    sval = ((1 << (length)) + (info) - 1);
    if (isSigned)
    {
        if (sval & 1)
            *pDst = (int32_t) ((sval + 1) >> 1);
        else
            *pDst = -((int32_t) (sval >> 1));
    }
    else
        *pDst = (int32_t) sval;

    return true;
}

// Read variable length coded unsigned element
inline uint32_t MPEG2BaseBitstream::GetVLCElementU()
{
    int32_t sval = 0;

    bool res = DecodeExpGolombOne_MPEG2_1u32s(&m_pbs, &m_bitOffset, &sval, false);

    if (!res)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    return (uint32_t)sval;
}

// Read variable length coded signed element
inline int32_t MPEG2BaseBitstream::GetVLCElementS()
{
    int32_t sval = 0;

    bool res = DecodeExpGolombOne_MPEG2_1u32s(&m_pbs, &m_bitOffset, &sval, true);

    if (!res)
        throw mpeg2_exception(UMC::UMC_ERR_INVALID_STREAM);

    return sval;
}

// Read one bit
inline uint8_t MPEG2BaseBitstream::Get1Bit()
{
    uint32_t w;

    GetBits1(m_pbs, m_bitOffset, w);
    return (uint8_t)w;

} // MPEG2Bitstream::Get1Bit()

// Read one bit
inline uint8_t MPEG2BaseBitstream::Check1Bit()
{
    uint32_t w;

    CheckBit1(m_pbs, m_bitOffset, w);
    return (uint8_t)w;

} // MPEG2Bitstream::Get1Bit()

// Returns number of decoded bytes since last reset
inline size_t MPEG2BaseBitstream::BytesDecoded() const
{
    return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase) +
            ((31 - m_bitOffset) >> 3);
}

// Returns number of decoded bits since last reset
inline size_t MPEG2BaseBitstream::BitsDecoded() const
{
    return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase) * 8 +
        (31 - m_bitOffset);
}

// Returns number of bytes left in bitstream array
inline size_t MPEG2BaseBitstream::BytesLeft() const
{
    return (int32_t)m_maxBsSize - (int32_t) BytesDecoded();
}

// Returns number of bits needed for byte alignment
inline unsigned MPEG2BaseBitstream::getNumBitsUntilByteAligned() const
{
    return ((m_bitOffset + 1) % 8);
}

// Align bitstream to byte boundary
inline void MPEG2BaseBitstream::readOutTrailingBits()
{
    Get1Bit();
    //VM_ASSERT(1 == uVal);

    uint32_t bits = getNumBitsUntilByteAligned();

    if (bits)
    {
        GetBits(bits);
        //VM_ASSERT(0 == uVal);
    }
}

// Align bitstream position to byte boundary
inline void MPEG2BaseBitstream::AlignPointerRight(void)
{
    if ((m_bitOffset & 0x07) != 0x07)
    {
        m_bitOffset = (m_bitOffset | 0x07) - 8;
        if (m_bitOffset == -1)
        {
            m_bitOffset = 31;
            m_pbs++;
        }
    }
}

} // namespace UMC_MPEG2_DECODER


#endif // __UMC_MPEG2_BITSTREAM_HEADERS_H_
#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
