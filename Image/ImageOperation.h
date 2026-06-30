#ifndef __ImageOperation_H_
#define __ImageOperation_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Image.h"
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GenerateMipLevel( CImage *pRes, const CImage &src, bool bWrap );
bool GenerateMipLevelPoint( CImage *pRes, const CImage &src );
void GenerateNormals( CImage *pData, const CVec4 &conv, float fMappingSize, bool bWrap );
bool LoadImage( CImage *pRes, CDataStream *pStream );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
