#ifndef __IMAGEMMP_H__
#define __IMAGEMMP_H__
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// binary format
class CImageMMP: public CObjectBase
{
	OBJECT_BASIC_METHODS( CImageMMP );
	int nSizeX, nSizeY;
	NGfx::EPixelFormat format;
	DWORD dwAverageColor;
	std::vector< std::vector<BYTE> > mips;
public:
	CImageMMP() {}
	CImageMMP( int nSizeX, int nSizeY, NGfx::EPixelFormat format, DWORD _dwAverageColor );

	int GetSizeX( int nMipLevel ) const { return nSizeX >> nMipLevel; }
	int GetSizeY( int nMipLevel ) const { return nSizeY >> nMipLevel; }
	int GetNumMipLevels() const { return mips.size(); }

	int GetBPP() const;
	NGfx::EPixelFormat GetFormat() const { return format; }
	DWORD GetAverageColor() const { return dwAverageColor; }

	const void* GetLFB( int nMipLevel ) const { return &( mips[nMipLevel][0] ); }
	int GetLinearSize( int nMip ) { return mips[nMip].size(); }
	int GetPitchInBytes( int nMip ) { return ( nSizeX >> nMip ) * GetBPP() / 8; }

	bool AddMipLevel( const void *pData, int nLength );
	void DropLargestMip();
	int GetMipsNumber() const { return mips.size(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SaveImageAsMMP( CDataStream *pStream, CImageMMP *pImage );
bool RecognizeFormatMMP( CDataStream *pStream );
CImageMMP* LoadImageMMP( CDataStream *pStream );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IMAGEMMP_H__
