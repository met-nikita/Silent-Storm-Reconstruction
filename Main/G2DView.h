#ifndef __G2DVIEW_H_
#define __G2DVIEW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Time.h"
#include "GText.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectLayout;
class CTransformStack;
namespace NGfx
{
	CVec2 GetScreenRect();
}
namespace NDb
{
	class CTexture;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRects;
class CTextLocaleInfo;
//////////////////////////////////////////////////////////////////////////////////////	
class CText: public CObjectBase
{
	OBJECT_BASIC_METHODS(CText);
public:
	vector< CObj<CRects> > rects;
	CDGPtr< CFuncBase< int > > pnfHeight;

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class I2DGameView: public CObjectBase
{
public:
	virtual CRects* CreateRects( NDb::CTexture *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 ) = 0;
	virtual CRects* CreateFullRect( NDb::CTexture *pTexture, CFuncBase< CTRect<int> > *pSize = 0 ) = 0;
	virtual CRects* CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 ) = 0;

	virtual void CreateDynamicRects( CFuncBase<SText> *pText, const CTPoint<int> &sPosition, const CTRect<int> &sWindow ) = 0;
	virtual void CreateDynamicRects( NDb::CTexture *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow ) = 0;
	virtual void CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow ) = 0;
	virtual CFuncBase<SText>* CreateText( CFuncBase<wstring> *pText, CFuncBase< CTPoint<int> > *pSize, bool bProcessTAGs = true ) = 0;
	virtual void CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ = 1.0f ) = 0;

	virtual CTextLocaleInfo* GetLocaleInfo() const = 0;
	virtual const CVec2& GetViewportSize() = 0;
	virtual void StartNewFrame() = 0;
	virtual void Flush() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
I2DGameView* CreateNew2DView();
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Is3DActive();
void SetWireframe( bool bWire );
void SetShowSceneInfo( bool bShow );
void Flip();
void ClearScreen( const CVec3 &vColor );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
