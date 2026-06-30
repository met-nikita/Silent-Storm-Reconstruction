#include "StdAfx.h"
#include "Cruncher.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor -- network LZ/range codec (.\release\Cruncher.obj). Reconstructed
// from the Game.exe decompilation against the real CBitLocker bit IO. The four
// static prefix-code tables are populated once at module load by SNetCompressorInit.
////////////////////////////////////////////////////////////////////////////////////////////////////
int CNetCompressor::nLengthBits[34];
int CNetCompressor::nLengthBitsNum[34];
int CNetCompressor::nShiftBits[34];
int CNetCompressor::nShiftBitsNum[34];
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::CNetCompressor  @0x3f5700
// Note (release-faithful): the ctor zeroes nHashTable in full, seeds nNext[0] and
// only four cBuffer cells; cBuffer/nNext bulk and nBlockLength are intentionally
// left as-is (nBlockLength is always assigned before it is read).
CNetCompressor::CNetCompressor()
{
	cPrevLetter = 0;
	nCurrent    = 0;
	nBlockStart = -1;
	nNext[0]    = 0x400;
	memset( nHashTable, 0, sizeof( nHashTable ) );
	cBuffer[0]     = 0;
	cBuffer[0x400] = 0;
	cBuffer[0x3ff] = 0;
	cBuffer[0x7ff] = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::EmitBlock  @0x3f54c0
// Encode the currently open match: the length code (table-driven for L<=0x22, then
// two explicit prefix ranges) followed by the distance/gap code (table-driven for
// gap<0x22, then four explicit prefix ranges). Closes the block (nBlockStart=-1).
void CNetCompressor::EmitBlock( int nStart )
{
	int len = nBlockLength;
	nBlockStart = -1;
	unsigned int gap = ( (unsigned int)nCurrent - len - nStart - 1 ) & 0x3ff;

	// ---- match length ----
	if ( len <= 0x22 )
		data.WriteBits( nLengthBits[len - 1], nLengthBitsNum[len - 1] );
	else if ( len <= 0x62 )
	{
		data.WriteBits( 0x17, 5 );
		data.WriteBits( len - 0x23, 6 );
	}
	else if ( len <= 0xe2 )
	{
		data.WriteBits( 0x3f, 6 );
		data.WriteBits( len - 0x63, 7 );
	}
	// (len > 0xe2 never happens: matches are capped at 0xe2.)

	// ---- match distance (gap) ----
	if ( gap < 0x22 )
		data.WriteBits( nShiftBits[gap], nShiftBitsNum[gap] );
	else if ( gap < 0x62 )
	{
		data.WriteBits( 2, 3 );
		data.WriteBits( gap - 0x22, 6 );
	}
	else if ( gap < 0x162 )
	{
		data.WriteBits( 6, 3 );
		data.WriteBits( gap - 0x62, 8 );
	}
	else if ( gap < 0x562 )
	{
		data.WriteBits( 3, 2 );
		data.WriteBits( gap - 0x162, 10 );
	}
	else if ( gap < 0x1562 )
	{
		data.WriteBits( 1, 2 );
		data.WriteBits( gap - 0x562, 0xc );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::StartPack  @0x3f56b0 -- seed the ring with one input byte.
void CNetCompressor::StartPack( unsigned char c )
{
	nNext[nCurrent] = 0x400;
	nCurrent = ( nCurrent + 1 ) & 0x3ff;
	cBuffer[nCurrent]         = c;
	cBuffer[nCurrent + 0x400] = c;
	cPrevLetter = c;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::EmitChar  @0x3f5630 -- flush the current byte as a literal.
void CNetCompressor::EmitChar()
{
	nBlockStart = -1;
	data.WriteBit( 0 );
	unsigned char ch = (unsigned char)cPrevLetter;
	data.Write( &ch, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::FinishPack  @0x3f5660 -- terminate a packing run.
void CNetCompressor::FinishPack()
{
	if ( nBlockStart >= 0 )
	{
		EmitBlock( nBlockStart - 1 );
		return;
	}
	nBlockStart = -1;
	data.WriteBit( 0 );
	unsigned char ch = (unsigned char)cPrevLetter;
	data.Write( &ch, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::StorePack  @0x3f5770
// "Stored" (uncompressed) pack: [u32 size] then a flag-0 literal for every input
// byte -- a valid Unpack stream containing no matches. Does not touch the LZ ring.
void CNetCompressor::StorePack( CDataStream &src, CDataStream &dst )
{
	CBitLocker in;
	src.Seek( 0 );
	int nSize = src.GetSize();
	in.LockRead( src, nSize );
	const unsigned char *p    = in.GetCurrentPtr();
	const unsigned char *pEnd = p + nSize;

	data.LockWrite( dst, ( nSize * 9 ) / 8 + 10 );
	data.Write( &nSize, 4 );
	for ( ; p < pEnd; ++p )
	{
		data.WriteBit( 0 );
		data.Write( p, 1 );
	}
	data.Free();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::Pack  @0x3f58d0
// LZ pack src -> dst. src is read-locked (the whole buffer is scanned through its
// locked pointer); dst is write-locked through `data`. Output = [u32 size][coded].
void CNetCompressor::Pack( CDataStream &src, CDataStream &dst )
{
	CBitLocker in;
	src.Seek( 0 );
	int nSize = src.GetSize();
	in.LockRead( src, nSize );
	const unsigned char *p    = in.GetCurrentPtr();
	const unsigned char *pEnd = p + nSize;

	data.LockWrite( dst, ( nSize * 9 ) / 8 + 10 );
	data.Write( &nSize, 4 );

	if ( nSize > 0 )
	{
		unsigned char b = *p;
		nNext[nCurrent] = 0x400;
		nCurrent = ( nCurrent + 1 ) & 0x3ff;
		cBuffer[nCurrent]         = b;
		cBuffer[nCurrent + 0x400] = b;
		cPrevLetter = b;

		while ( ++p < pEnd )
		{
			b = *p;
			unsigned int h    = cPrevLetter * 4 ^ (unsigned int)b;
			int          iOld = nCurrent;                       // ring pos before advance
			unsigned int nLast = nHashTable[h];
			nHashTable[h] = (unsigned int)iOld;
			unsigned int gap = ( (unsigned int)iOld - nLast ) & 0x3ff;
			if ( gap == 0 ) gap = 0x400;
			nNext[iOld] = gap;
			nCurrent = ( iOld + 1 ) & 0x3ff;
			cBuffer[nCurrent]         = b;
			cBuffer[nCurrent + 0x400] = b;

			int nStart = nBlockStart;
			if ( nStart < 0 )
			{
				// no open match: look for a 2-byte (prev,cur) pair in the ring
				unsigned short pair;
				memcpy( &pair, p - 1, 2 );
				nBlockStart = iOld;
				bool bFound = false;
				if ( gap < 0x31e )
				{
					do
					{
						unsigned int pos = ( iOld - gap ) & 0x3ff;
						unsigned short cand;
						memcpy( &cand, cBuffer + pos, 2 );
						if ( cand == pair )
						{
							nBlockStart  = (int)pos;
							nBlockLength = 2;
							bFound = true;
							break;
						}
						gap += nNext[pos];
					} while ( (int)gap < 0x31e );
				}
				if ( !bFound )
				{
					nBlockStart = -1;
					data.WriteBit( 0 );
					unsigned char ch = (unsigned char)cPrevLetter;
					data.Write( &ch, 1 );
				}
			}
			else
			{
				// open match: try to extend by one, else re-anchor to another
				// occurrence of (block + cur), else emit the block.
				bool bExtended = false;
				if ( nBlockLength < 0xe1 )
				{
					if ( cBuffer[nBlockLength + nStart] == b )
					{
						++nBlockLength;
						bExtended = true;
					}
					else
					{
						unsigned int nFrom = (unsigned int)( nCurrent - nBlockLength );
						int d = (int)( ( nFrom - nStart ) & 0x3ff ) + (int)nNext[nStart];
						for ( ; d < 0x31e; d += (int)h )
						{
							while ( (int)gap < d )
								gap += nNext[( iOld - gap ) & 0x3ff];
							if ( d < (int)gap )
							{
								h = nNext[( nFrom - d ) & 0x3ff];
							}
							else
							{
								unsigned int pos = ( nFrom - d ) & 0x3ff;
								int n = nBlockLength + 1;
								bool ok = true;
								const unsigned char *a = cBuffer + pos;
								const unsigned char *c = cBuffer + ( nFrom & 0x3ff );
								while ( n != 0 )
								{
									--n;
									ok = ( *a == *c );
									++a; ++c;
									if ( !ok ) break;
								}
								if ( ok )
								{
									nBlockStart = (int)pos;
									++nBlockLength;
									bExtended = true;
									break;
								}
								h = nNext[pos];
								gap += nNext[( iOld - gap ) & 0x3ff];
							}
						}
						if ( !bExtended ) nStart = nBlockStart;
					}
				}
				if ( !bExtended ) EmitBlock( nStart );
			}
			cPrevLetter = b;
		}

		// tail: flush the trailing literal or close the open match
		if ( nBlockStart < 0 )
		{
			nBlockStart = -1;
			data.WriteBit( 0 );
			unsigned char ch = (unsigned char)cPrevLetter;
			data.Write( &ch, 1 );
		}
		else
		{
			EmitBlock( nBlockStart - 1 );
		}
	}
	data.Free();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// match-length / match-distance prefix decoders (inverse of EmitBlock's encoding).
// Read LSB-first off the source bit stream; validated against real packed saves.
static int ReadMatchLength( CBitStream &br )
{
	if ( !br.ReadBit() ) return br.ReadBit() ? 4 : 2;
	if ( !br.ReadBit() )
	{
		if ( !br.ReadBit() ) return br.ReadBit() ? (int)br.ReadBits( 3 ) + 0xb : 3;
		return (int)br.ReadBits( 2 ) + 7;
	}
	if ( !br.ReadBit() ) return br.ReadBit() ? (int)br.ReadBits( 6 ) + 0x23 : 5;
	if ( !br.ReadBit() ) return 6;
	return br.ReadBit() ? (int)br.ReadBits( 7 ) + 99 : (int)br.ReadBits( 4 ) + 0x13;
}
static unsigned int ReadMatchDistance( CBitStream &br )
{
	if ( br.ReadBit() )
		return br.ReadBit() ? br.ReadBits( 10 ) + 0x162 : br.ReadBits( 12 ) + 0x562;
	if ( br.ReadBit() )
		return br.ReadBit() ? br.ReadBits( 8 ) + 0x62 : br.ReadBits( 6 ) + 0x22;
	if ( !br.ReadBit() )
		return br.ReadBit() ? br.ReadBits( 2 ) + 2 : br.ReadBits( 3 ) + 10;
	if ( !br.ReadBit() ) return br.ReadBits( 2 ) + 6;
	return br.ReadBit() ? br.ReadBits( 1 ) : br.ReadBits( 4 ) + 0x12;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetCompressor::Unpack  @0x3f5cb0
// Inverse of Pack. src is read-locked at its CURRENT position (the caller has it
// parked on the packed chunk); dst is write-locked through `data`. The 4-byte
// header is the uncompressed size; literals and ring-back-reference matches follow.
void CNetCompressor::Unpack( CDataStream &src, CDataStream &dst )
{
	CBitLocker in;
	int nRemain = src.GetSize() - src.GetPosition();
	in.LockRead( src, nRemain );

	int nLeft = 0;
	in.Read( &nLeft, 4 );
	data.LockWrite( dst, nLeft );

	while ( nLeft > 0 )
	{
		if ( in.ReadBit() == 0 )
		{
			unsigned char b;
			in.Read( &b, 1 );
			data.Write( &b, 1 );
			cBuffer[nCurrent] = b;
			--nLeft;
			nCurrent = ( nCurrent + 1 ) & 0x3ff;
		}
		else
		{
			int          len  = ReadMatchLength( in );
			unsigned int dist = ReadMatchDistance( in );
			if ( nLeft <= len ) len = nLeft;
			for ( int i = 0; i < len; ++i )
			{
				unsigned char c = cBuffer[( nCurrent - dist - 1 ) & 0x3ff];
				data.Write( &c, 1 );
				cBuffer[nCurrent] = c;
				nCurrent = ( nCurrent + 1 ) & 0x3ff;
			}
			nLeft -= len;
		}
	}
	data.Free();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SNetCompressorInit::SNetCompressorInit  @0x3f5170
// Fills CNetCompressor's four static prefix-code tables. Pure deterministic table
// generation transcribed verbatim from the binary (strength-reduced running
// counters and all) so every entry is byte-identical.
SNetCompressorInit::SNetCompressorInit()
{
	// ---- Loop 1: match-LENGTH tables (nLengthBits / nLengthBitsNum), 34 entries ----
	{
		int cnt10 = -0x421;   // code-word counter for the 10-bit (len 19..34) range
		int cnt8  = -0x10d;   // code-word counter for the  8-bit (len 11..18) range
		int cnt6  = -0x45;    // code-word counter for the  6-bit (len  7..10) range
		int e = 0;            // entry index (steps by 2)
		int n = 2;            // even length value: 2,4,...,34
		for ( int count = 0x11; count != 0; --count )   // 17 iterations
		{
			const int v = n - 1;                        // odd length value: 1,3,...,33

			// odd length value -> slot e
			CNetCompressor::nLengthBits[e] = 0;
			if      ( v < 2 )    { CNetCompressor::nLengthBits[e] = 0;    CNetCompressor::nLengthBitsNum[e] = 1; }
			else if ( v < 3 )    { CNetCompressor::nLengthBits[e] = 1;    CNetCompressor::nLengthBitsNum[e] = 3; }
			else if ( v < 4 )    { CNetCompressor::nLengthBits[e] = 3;    CNetCompressor::nLengthBitsNum[e] = 5; }
			else if ( v < 5 )    { CNetCompressor::nLengthBits[e] = 5;    CNetCompressor::nLengthBitsNum[e] = 3; }
			else if ( v < 6 )    { CNetCompressor::nLengthBits[e] = 7;    CNetCompressor::nLengthBitsNum[e] = 5; }
			else if ( v < 7 )    { CNetCompressor::nLengthBits[e] = 0xf;  CNetCompressor::nLengthBitsNum[e] = 5; }
			else if ( v < 0xb )  { CNetCompressor::nLengthBitsNum[e] = 6;  CNetCompressor::nLengthBits[e] = cnt6  - 0x10; }
			else if ( v < 0x13 ) { CNetCompressor::nLengthBitsNum[e] = 8;  CNetCompressor::nLengthBits[e] = cnt8  - 0x20; }
			else if ( v < 0x23 ) { CNetCompressor::nLengthBitsNum[e] = 10; CNetCompressor::nLengthBits[e] = cnt10 - 0x40; }

			// even length value -> slot e+1
			CNetCompressor::nLengthBits[e + 1] = 0;
			if      ( n < 2 )    { CNetCompressor::nLengthBits[e + 1] = 0;   CNetCompressor::nLengthBitsNum[e + 1] = 1; }
			else if ( n < 3 )    { CNetCompressor::nLengthBits[e + 1] = 1;   CNetCompressor::nLengthBitsNum[e + 1] = 3; }
			else if ( n < 4 )    { CNetCompressor::nLengthBits[e + 1] = 3;   CNetCompressor::nLengthBitsNum[e + 1] = 5; }
			else if ( n < 5 )    { CNetCompressor::nLengthBits[e + 1] = 5;   CNetCompressor::nLengthBitsNum[e + 1] = 3; }
			else if ( n < 6 )    { CNetCompressor::nLengthBits[e + 1] = 7;   CNetCompressor::nLengthBitsNum[e + 1] = 5; }
			else if ( n < 7 )    { CNetCompressor::nLengthBits[e + 1] = 0xf; CNetCompressor::nLengthBitsNum[e + 1] = 5; }
			else if ( n < 0xb )  { CNetCompressor::nLengthBitsNum[e + 1] = 6;  CNetCompressor::nLengthBits[e + 1] = cnt6; }
			else if ( n < 0x13 ) { CNetCompressor::nLengthBitsNum[e + 1] = 8;  CNetCompressor::nLengthBits[e + 1] = cnt8; }
			else if ( n < 0x23 ) { CNetCompressor::nLengthBitsNum[e + 1] = 10; CNetCompressor::nLengthBits[e + 1] = cnt10; }

			cnt6  += 0x20;
			cnt8  += 0x40;
			cnt10 += 0x80;
			e += 2;
			n += 2;
		}
	}

	// ---- Loop 2: match-DISTANCE tables (nShiftBits / nShiftBitsNum), 34 entries ----
	{
		int e  = 0;          // entry index, steps by 2
		int c2 = -0x234;     // secondary counter (step 0x40)
		int c  = -0x4c;      // primary counter   (step 0x20)
		for ( ;; )
		{
			// even slot e (branch on the primary counter c)
			CNetCompressor::nShiftBits[e] = 0;
			if      ( c < -0x2c ) { CNetCompressor::nShiftBitsNum[e] = 6; CNetCompressor::nShiftBits[e] = c2 + 0x250; }
			else if ( c < 0x14 )  { CNetCompressor::nShiftBitsNum[e] = 6; CNetCompressor::nShiftBits[e] = c  + 0x34; }
			else if ( c < 0x54 )  { CNetCompressor::nShiftBitsNum[e] = 6; CNetCompressor::nShiftBits[e] = c  - 0x10; }
			else if ( c < 0xd4 )  { CNetCompressor::nShiftBitsNum[e] = 7; CNetCompressor::nShiftBits[e] = c  - 0x54; }
			else if ( c < 0x1d4 ) { CNetCompressor::nShiftBitsNum[e] = 9; CNetCompressor::nShiftBits[e] = c2; }

			// odd slot e+1 (branch on the raw slot index)
			const int idx = e + 1;
			CNetCompressor::nShiftBits[e + 1] = 0;
			if      ( idx < 2 )    { CNetCompressor::nShiftBitsNum[e + 1] = 6; CNetCompressor::nShiftBits[e + 1] = c2 + 0x270; }
			else if ( idx < 6 )    { CNetCompressor::nShiftBitsNum[e + 1] = 6; CNetCompressor::nShiftBits[e + 1] = c  + 0x44; }
			else if ( idx < 0xa )  { CNetCompressor::nShiftBitsNum[e + 1] = 6; CNetCompressor::nShiftBits[e + 1] = c; }
			else if ( idx < 0x12 ) { CNetCompressor::nShiftBitsNum[e + 1] = 7; CNetCompressor::nShiftBits[e + 1] = c  - 0x44; }
			else if ( idx < 0x22 ) { CNetCompressor::nShiftBitsNum[e + 1] = 9; CNetCompressor::nShiftBits[e + 1] = c2 + 0x20; }

			c  += 0x20;
			e  += 2;
			c2 += 0x40;
			if ( 0x1d3 < c ) break;   // post-increment termination (entries 0..33)
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// file-scope static: runs SNetCompressorInit's ctor once at module load.
static SNetCompressorInit g_NetCompressorInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
