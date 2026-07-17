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
	// retail C2DGameView::operator& @0xd90b0 serializes ONLY {2=pScene, 3=pLocale}. pScreenRect is a
	// device-bound LIVE resource (the viewport-size CCVec2 func node) that must NOT round-trip -- the
	// default ctor rebuilds it (new CCVec2(GetScreenRect()), and StartNewFrame keeps it synced). The
	// old dev leg wrote it at tag 2 and shifted pScene/pLocale up, so loading a retail save deserialized
	// pScene's bytes INTO the CDGPtr pScreenRect (nulling it) -> GetViewportSize() null-derefs on the
	// first post-load frame (UIInterface.cpp:461). Match retail: drop pScreenRect, keep the ctor value.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pScene); f.Add(3,&pLocale); return 0; }

public:
	C2DGameView();

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
