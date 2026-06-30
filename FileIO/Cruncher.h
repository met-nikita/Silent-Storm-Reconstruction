#ifndef __CRUNCHER_H_
#define __CRUNCHER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Network LZ/range compressor (release compiland .\release\Cruncher.obj).
//
// LZ77 over a 0x400-byte ring (mirrored at +0x400 so a 2-byte match compare never
// wraps), bit codes interleaved with raw bytes in ONE CBitLocker stream. Stream =
// [u32 uncompressed-size][coded data]. The codec is driven by CBitStream's
// LSB-first bit IO and the static prefix-code tables filled once at module load by
// SNetCompressorInit (see Cruncher.cpp). This is the codec that BasicChunk1's
// packed save/load path (CStructureSaver) consumes.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Streams.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNetCompressorInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNetCompressor
{
	// ---- PDB-exact instance layout (size 10280, x86) ----
	unsigned char cBuffer[2048];     // +0x0000  LZ ring, mirrored: [0..0x3ff] == [0x400..0x7ff]
	unsigned int  nNext[1024];       // +0x0800  hash-chain gap deltas, indexed by ring pos
	unsigned int  nHashTable[1024];  // +0x1800  (prevByte*4 ^ byte) -> last ring pos
	CBitLocker    data;              // +0x2800  active write/read lock for the coded stream
	int           nCurrent;          // +0x2818  ring write cursor
	int           nBlockStart;       // +0x281c  open match start (-1 == none)
	int           nBlockLength;      // +0x2820  open match length
	unsigned int  cPrevLetter;       // +0x2824  previous input byte

	// ---- static prefix-code tables (each int[34]; filled by SNetCompressorInit) ----
	// nLengthBits[L-1]/nLengthBitsNum[L-1] : match-length L code word / #bits.
	// nShiftBits[d]/nShiftBitsNum[d]       : match-distance d code word / #bits.
	static int nLengthBits[34];
	static int nLengthBitsNum[34];
	static int nShiftBits[34];
	static int nShiftBitsNum[34];

	void EmitBlock( int nStart );    // @0x3f54c0  encode the open match (len + gap)

	friend struct SNetCompressorInit;
public:
	CNetCompressor();                                 // @0x3f5700

	// one-shot whole-stream packing (src is read-locked, dst is write-locked):
	void Pack( CDataStream &src, CDataStream &dst );       // @0x3f58d0  LZ pack
	void Unpack( CDataStream &src, CDataStream &dst );     // @0x3f5cb0  inverse of Pack
	void StorePack( CDataStream &src, CDataStream &dst );  // @0x3f5770  "stored" (no LZ) pack

	// incremental packing helpers (operate on the already-locked `data`):
	void StartPack( unsigned char c );                // @0x3f56b0  seed the ring
	void EmitChar();                                  // @0x3f5630  flush trailing literal
	void FinishPack();                                // @0x3f5660  terminate a packing run
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// 1-byte tag whose constructor fills CNetCompressor's four static code tables once,
// at module load (a single file-scope static instance lives in Cruncher.cpp).
struct SNetCompressorInit
{
	SNetCompressorInit();                             // @0x3f5170
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __CRUNCHER_H_
