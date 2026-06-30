#ifndef __ImagePack_H_
#define __ImagePack_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Image.h"
namespace NImage
{
	class CImageMMP;
	CImageMMP* Pack( const SMippedImage &src, NGfx::EPixelFormat format );
}
#endif
