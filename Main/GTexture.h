#ifndef __GTEXTURE_H__
#define __GTEXTURE_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GResource.h"
#include "GTextureLoader.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CTexture;
	class CCubeTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STextureKey
{
	enum EEFlags
	{
		TK_WRAP = 1,
		TK_TRANSPARENT = 2
	};
	int nID;
	int nFlags;

	STextureKey() {}
	STextureKey( int _nID, int _nFlags = 0 ): nID(_nID), nFlags(_nFlags) {}

	bool operator==( const STextureKey &a ) const { return nID == a.nID && nFlags == a.nFlags; }
};
struct STextureKeyHash
{
	int operator()( const STextureKey &k ) const { return k.nID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// texture loader from disk
class CFileTexture : public CResourceLoader<STextureKey, NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CFileTexture);
	typedef CResourceLoader<STextureKey, NGfx::CTexture> TParent;
	bool bIsFakeTexture;
	CObj<CFileRequest> pRequest;
protected:
	virtual void Recalc();
	virtual bool NeedUpdate();
public:
	CFileTexture() : bIsFakeTexture(false) {}
	void CreateChecker();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileCubeTexture : public CResourceLoader<int, NGfx::CCubeTexture>
{
	OBJECT_BASIC_METHODS(CFileCubeTexture);
protected:
	virtual void Recalc();
public:
	void CreateChecker();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CColorTexture : public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CColorTexture);
	ZDATA
	CVec4 vColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vColor); return 0; }
protected:
	virtual void Recalc();
public:
	CColorTexture() {}
	CColorTexture( const CVec4 &_v ) : vColor(_v) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GTEXTURE_H__
