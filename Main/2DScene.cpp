#include "StdAfx.h"
#include "GClipper.h"
#include "GRects.h"
#include "2DScene.h"
#include "GfxUtils.h"
#include "GSceneUtils.h"
#include "Transform.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRects: public CObjectBase
{
	OBJECT_BASIC_METHODS(CRects);
public:
	ZDATA
	bool bClearRects;
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTexture;
	CDGPtr<CFuncBase<CRectLayout> > pLayout;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bClearRects); f.Add(3,&pTexture); f.Add(4,&pLayout); return 0; }
	//
	CRects() {}
	CRects( bool _bClearRects, CPtrFuncBase<NGfx::CTexture> *_pTex, CFuncBase<CRectLayout> *_pLay )
		: bClearRects(_bClearRects), pTexture(_pTex), pLayout(_pLay) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class C2DScene: public I2DScene
{
	OBJECT_NOCOPY_METHODS(C2DScene);

	NGfx::C2DQuadsRenderer quadRender;
	ZDATA
	list< CPtr<CRects> > rects;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&rects); return 0; }

public:
	CRects* CreateRects( CPtrFuncBase<NGfx::CTexture> *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize );
	CRects* CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize );

	void CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow );
	void CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ = 1.0f );

	void StartNewFrame( NGfx::CTexture *pTarget, const CVec2 &vSize );
	void Flush();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// C2DScene
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPosNode: public CFuncBase< CTPoint<int> >
{
	OBJECT_BASIC_METHODS(CPosNode);
protected:
	virtual bool NeedUpdate() { return pRect.Refresh(); }
	virtual void Recalc() { value = CTPoint<int>( pRect->GetValue().x1, pRect->GetValue().y1 ); }
public:
	ZDATA
	CDGPtr< CFuncBase< CTRect<int> > > pRect;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pRect); return 0; }
	//
	CPosNode() {}
	CPosNode( CFuncBase< CTRect<int> > *_pRect ): pRect( _pRect ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRects* C2DScene::CreateRects( CPtrFuncBase<NGfx::CTexture> *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize )
{
	CFuncBase<CRectLayout> *pResLayout = pLayout;
	if ( pSize )
	{
		CClipper *pClipper = new CClipper;
		pClipper->pRects = pLayout;
		pClipper->pWindow = pSize;
		pClipper->pPosition = new CPosNode( pSize );
		pResLayout = pClipper;
	}
	CRects *pRes = new CRects( false, pTexture, pResLayout );
	rects.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRects* C2DScene::CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize )
{
	CRects *pRes = CreateRects( 0, pLayout, pSize );
	pRes->bClearRects = true;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CRectLayout clippedTemp;
void C2DScene::CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow )
{
//	ClipRects( &clippedTemp, sLayout, sWindow, CVec2(sPosition.x, sPosition.y) );
	if ( pTexture )
	{
		CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTex( pTexture );
		pTex.Refresh();
		RenderRectLayoutClipped( &quadRender, pTex->GetValue(), sLayout, sPosition, sWindow, 0.0f, LRM_NORMAL );
	}
	else
		RenderRectLayoutClipped( &quadRender, 0, sLayout, sPosition, sWindow, 0.0f, LRM_NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow, float fZ )
{
//	ClipRects( &clippedTemp, sLayout, sWindow, CVec2(sPosition.x, sPosition.y) );
	RenderRectLayoutClipped( &quadRender, 0, sLayout, sPosition, sWindow, fZ, LRM_CLEAR_RECT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::StartNewFrame( NGfx::CTexture *pTarget, const CVec2 &vSize )
{
	NGfx::CRenderContext rc;
	if ( pTarget ) 
		rc.SetTextureRT( pTarget, 0 );
	rc.SetAlphaCombine( NGfx::COMBINE_ALPHA );
	rc.SetStencil( NGfx::STENCIL_NONE );
	quadRender.SetTarget( rc, vSize, NGfx::QRM_OVERWRITE );

	for ( list< CPtr<CRects> >::iterator i = rects.begin(); i != rects.end(); )
	{
		CRects *pRect = *i;
		if ( IsValid( pRect ) )
		{
			pRect->pLayout.Refresh();
			if ( IsValid( pRect->pTexture ) )
			{
				pRect->pTexture.Refresh();
				float fZ = pRect->bClearRects ? (float)0 : (float)1;
				ELayoutRenderMode lrm = pRect->bClearRects ? LRM_CLEAR_RECT : LRM_NORMAL;
				RenderRectLayout( &quadRender, pRect->pTexture->GetValue(), pRect->pLayout->GetValue(), fZ, lrm );
			}
			else
			{
				float fZ = pRect->bClearRects ? (float)0 : (float)1;
				ELayoutRenderMode lrm = pRect->bClearRects ? LRM_CLEAR_RECT : LRM_NORMAL;
				RenderRectLayout( &quadRender, 0, pRect->pLayout->GetValue(), fZ, lrm );
			}
			++i;
		}
		else
			i = rects.erase( i );
	}
	//Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::Flush()
{
	quadRender.Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Make scene
////////////////////////////////////////////////////////////////////////////////////////////////////
I2DScene* Make2DScene()
{
	return new C2DScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESOACE
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xF2005170, CPosNode );
REGISTER_SAVELOAD_CLASS( 0xF2005171, C2DScene );
REGISTER_SAVELOAD_CLASS( 0xF2005175, CRects );
