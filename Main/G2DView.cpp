#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GMemFormat.h"
#include "GFont.h"
#include "GLocale.h"
#include "GText.h"
#include "GTexture.h"
#include "GMemFormat.h"
#include "GMemBuilder.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "2DScene.h"
#include "G2DView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// share objects
CBasicShare<int, CFileFont> shareFonts(107);
externA5 CBasicShare<STextureKey, CFileTexture, STextureKeyHash> shareTextures;
////////////////////////////////////////////////////////////////////////////////////////////////////
class C2DGameView: public I2DGameView
{
	OBJECT_BASIC_METHODS(C2DGameView);
private:
	ZDATA
	CDGPtr<CCVec2> pScreenRect;
	CObj<I2DScene> pScene;
	CObj<CTextLocaleInfo> pLocale;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pScreenRect); f.Add(3,&pScene); f.Add(4,&pLocale); return 0; }

public:
	C2DGameView();
	
	CRects* CreateRects( NDb::CTexture *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 );
	CRects* CreateFullRect( NDb::CTexture *pTexture, CFuncBase< CTRect<int> > *pSize = 0 );
	CRects* CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 );

	void CreateDynamicRects( CFuncBase<SText> *pText, const CTPoint<int> &sPosition, const CTRect<int> &sWindow );
	void CreateDynamicRects( NDb::CTexture *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow );
	void CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow );
	CFuncBase<SText>* CreateText( CFuncBase<wstring> *pText, CFuncBase< CTPoint<int> > *pSize, bool bProcessTAGs = true );
	void CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ = 1.0f );

	virtual const CVec2& GetViewportSize() { return pScreenRect->GetValue(); }
	CTextLocaleInfo* GetLocaleInfo() const { return pLocale; }
	void StartNewFrame();
	void Flush();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// C2DGameView
////////////////////////////////////////////////////////////////////////////////////////////////////
C2DGameView::C2DGameView()
{
	pScene = Make2DScene();
	pScreenRect = new CCVec2( NGfx::GetScreenRect() );
	///
	pLocale = new CTextLocaleInfo;
	pLocale->Setup( NGfx::GetScreenRect() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRects* C2DGameView::CreateRects( NDb::CTexture *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize )
{
	return pScene->CreateRects( shareTextures.Get( pTexture->GetRecordID() ), pLayout, pSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRects* C2DGameView::CreateFullRect( NDb::CTexture *pTexture, CFuncBase< CTRect<int> > *pSize )
{
	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, pTexture->nHeight, pTexture->nWidth, 0 ) );
	CPtr<CCRectLayout> pLayout = new CCRectLayout;
	pLayout->Set( sLayout );

	return CreateRects( pTexture, pLayout, pSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRects* C2DGameView::CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize )
{
	return pScene->CreateClearRects( pLayout, pSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::CreateDynamicRects( CFuncBase<SText> *pText, const CTPoint<int> &sPosition, const CTRect<int> &sWindow )
{
	CDGPtr< CFuncBase<SText> > pFormater( pText );

	pFormater.Refresh();
	const SText &sText = pFormater->GetValue();
	for ( int nTemp = 0; nTemp < sText.rectLayouts.size(); nTemp++ )
	{
		const SText::SFontLayout &sLayout = sText.rectLayouts[nTemp];
		pScene->CreateDynamicRects( sLayout.pFontInfo->GetTexture(), sLayout.sLayout, sPosition, sWindow );
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::CreateDynamicRects( NDb::CTexture *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow )
{
	if ( !pTexture )
		pScene->CreateDynamicRects( 0, sLayout, sPosition, sWindow );
	else
		pScene->CreateDynamicRects( shareTextures.Get( pTexture->GetRecordID() ), sLayout, sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow )
{
	pScene->CreateDynamicRects( pTexture, sLayout, sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFuncBase<SText>* C2DGameView::CreateText( CFuncBase<wstring> *pText, CFuncBase< CTPoint<int> > *pSize, bool bProcessTAGs )
{
	return CreateTextFormater( pLocale, pScreenRect, pText, pSize, bProcessTAGs );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ )
{
	pScene->CreateDynamicClearRects( sLayout, sPosition, sClipWindow, fZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::StartNewFrame()
{
	pScreenRect.Refresh();
	if ( pScreenRect->GetValue() != NGfx::GetScreenRect() )
		pScreenRect->Set( NGfx::GetScreenRect() );
	pLocale->Setup( NGfx::GetScreenRect() );

	pScene->StartNewFrame( 0, NGfx::GetScreenRect() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DGameView::Flush()
{
	pScene->Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Create 2D View
////////////////////////////////////////////////////////////////////////////////////////////////////
I2DGameView* CreateNew2DView()
{
	return new C2DGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xF1741142, C2DGameView );
