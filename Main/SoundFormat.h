#ifndef __SOUNDFORMAT_H_
#define __SOUNDFORMAT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GResource.h"
namespace NFMSound
{
	class CSample3D;
	class CSample2D;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileSample3D: public NGScene::CLazyResourceLoader<int, NFMSound::CSample3D>
{
	OBJECT_BASIC_METHODS(CFileSample3D);
	virtual NGScene::CFileRequest* CreateRequest();
	virtual void RecalcValue( NGScene::CFileRequest *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NSound::CFileSoftwareSample3D (id 0xA1713090, ctor @0x3081c0, RecalcValue @0x30a590):
// the SOFTWARE-mixer 3D sample loader. Identical to CFileSample3D except it RETAINS the raw file
// request (pBufferHolder) after a successful decode -- retail's software mixer streams from that
// buffer. The wire is the inherited loader key only ({1:4}, byte-walked); pBufferHolder transient.
class CFileSoftwareSample3D: public NGScene::CLazyResourceLoader<int, NFMSound::CSample3D>
{
	OBJECT_BASIC_METHODS(CFileSoftwareSample3D);
	CObj<NGScene::CFileRequest> pBufferHolder;
	virtual NGScene::CFileRequest* CreateRequest();
	virtual void RecalcValue( NGScene::CFileRequest *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileSample2D: public NGScene::CResourceLoader<int, NFMSound::CSample2D>
{
	OBJECT_BASIC_METHODS(CFileSample2D);
protected:
	virtual void Recalc();	
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
