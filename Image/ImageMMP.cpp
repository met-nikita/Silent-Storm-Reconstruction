#include "StdAfx.h"
#include "ImageMMP.h"
#include "mmpFormat.h"
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NImage::SaveImageAsMMP( CDataStream *pStream, CImageMMP *pImage )
{
	SMMPFileHeader sHeader;
	sHeader.nSizeX = pImage->GetSizeX( 0 );
	sHeader.nSizeY = pImage->GetSizeY( 0 );
	sHeader.format = pImage->GetFormat();
	sHeader.dwAverageColor = pImage->GetAverageColor();
	sHeader.nNumMipLevels = pImage->GetNumMipLevels();

	pStream->Write( &sHeader, sizeof(SMMPFileHeader) );

	for ( int nTemp = 0; nTemp < pImage->GetNumMipLevels(); nTemp++ )
		pStream->Write( pImage->GetLFB( nTemp ), pImage->GetSizeX( nTemp ) * pImage->GetSizeY( nTemp ) * pImage->GetBPP() / 8 );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool RecognizeFormatMMP( CDataStream *pStream )
{
	try
	{
		pStream->Seek(0);
		SMMPFileHeader sHeader;
		Zero( sHeader );
		pStream->Read( &sHeader, sizeof(SMMPFileHeader ) );
		if ( sHeader.dwSignature == MMP_SIGNATURE )
			return true;
	}
	catch(...)
	{
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageMMP* LoadImageMMP( CDataStream *pStream )
{
	try
	{
		pStream->Seek(0);
		SMMPFileHeader h;
		Zero( h );
		pStream->Read( &h, sizeof(SMMPFileHeader ) );
		if ( h.dwSignature != MMP_SIGNATURE )
			return 0;
		CObj<CImageMMP> pImage = new CImageMMP( h.nSizeX, h.nSizeY, h.format, h.dwAverageColor );
		for ( int k = 0; k < h.nNumMipLevels; ++k )
		{
			vector<char> data( pImage->GetSizeX( k ) * pImage->GetSizeY( k ) * pImage->GetBPP() / 8, 0 );
			pStream->Read( &data[0], data.size() );
			pImage->AddMipLevel( &data[0], data.size() );
		}
		return pImage.Extract();
	}
	catch(...)
	{
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImageMMP
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageMMP::CImageMMP( int _nSizeX, int _nSizeY, NGfx::EPixelFormat _format, DWORD _dwAverageColor )
: nSizeX( _nSizeX ), nSizeY( _nSizeY ), format( _format ), dwAverageColor(_dwAverageColor)
{
	int nSize = Min( nSizeX, nSizeY );
	int nPow2 = GetMSB( nSize );
	mips.reserve( nPow2 + 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CImageMMP::GetBPP() const
{
	return NGfx::GetBPP( format );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImageMMP::AddMipLevel( const void *pData, int nLength )
{
	mips.resize( mips.size() + 1 );
	mips.back().resize( nLength );
	memcpy( &( mips.back()[0] ), pData, nLength );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageMMP::DropLargestMip()
{
	ASSERT( !mips.empty() );
	if ( mips.empty() )
		return;
	mips.erase( mips.begin() );
	ASSERT( (nSizeX & 1) == 0 );
	ASSERT( (nSizeY & 1) == 0 );
	nSizeX >>= 1;
	nSizeY >>= 1;
	ASSERT( !mips.empty() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*bool CImageMMP::AddMipLevels( const IMMPImage *pImage )
{
	if ( ( pImage->GetSizeX(0) != GetSizeX(mips.size()) ) || ( pImage->GetSizeY(0) != GetSizeY(mips.size()) ) )
		return false;
	int nSize = Min( nSizeX, nSizeY );
	int nMaxMips = GetMSB( nSize );
	if ( (format >= NGfx::CF_DXT1) && (format <= NGfx::CF_DXT5)  )
		nMaxMips -= 2;
	//
	int nNumMipLevels = Min( int(nMaxMips - mips.size()), pImage->GetNumMipLevels() );
	for ( int i=0; i<nNumMipLevels; ++i )
		AddMipLevel( pImage->GetLFB( i ), GetSizeX( mips.size() ) * GetSizeY( mips.size() ) * GetBPP() / 8 );

	return true;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}