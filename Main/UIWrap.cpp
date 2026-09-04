#include "StdAfx.h"
#include "GPixelFormat.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "GText.h"
#include "Interface.h"
#include "UIWrap.h"
#include "UIML.h"
#include "DiscretePos.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x329500: UI quads use premultiplied vertex colors. C2DScene pairs these with
// COMBINE_SMART_ALPHA (ONE/INVSRCALPHA), preserving ordinary translucency while allowing
// bright additive-looking assets such as the store category flash to remain visible.
void MakeColor( NGfx::SPixel8888 *pResult, const NGfx::SPixel8888 &sColor )
{
	pResult->r = (unsigned int)( sColor.r ) * sColor.a / 0xFF;
	pResult->g = (unsigned int)( sColor.g ) * sColor.a / 0xFF;
	pResult->b = (unsigned int)( sColor.b ) * sColor.a / 0xFF;
	pResult->a = sColor.a;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x329c20: sSize = sRealSize = size; sPosition = pos; nSize = 0; wsText(text);
// pML = CreateML(); pML->SetText(wsText, 0).
CTextDraw::CTextDraw( const SPoint &_sPosition, const SPoint &_sSize, const wstring &_wsText ):
	sPosition( _sPosition ), sSize( _sSize ), sRealSize( _sSize ), wsText(_wsText), nSize( 0 )
{
	pML = CreateML();
	pML->SetText( wsText, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GetSize @0x329a30: a dead/null view returns the requested sSize untouched; otherwise
// refresh the layout (UpdateText), copy requested -> resolved, and overwrite each -1 component with
// the layout's measured extent mapped back into virtual 1024x768 coordinates. NOTE retail always
// recomputes sRealSize on a live view (the old dev body only did so when a component was -1).
const SPoint& CTextDraw::GetSize( NGScene::I2DGameView *pView )
{
	if ( !IsValid( pView ) )
		return sSize;

	UpdateText( pView );

	const SPoint &sMLSize = pML->GetSize();
	const CVec2 &vScreenRect = pView->GetViewportSize();

	sRealSize = sSize;
	if ( sSize.x == -1 )
		sRealSize.x = sMLSize.x * 1024 / vScreenRect.x;
	if ( sSize.y == -1 )
		sRealSize.y = sMLSize.y * 768 / vScreenRect.y;

	return sRealSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDraw::SetSize( const SPoint &_sSize )
{
	sSize = _sSize;
	sRealSize = _sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CTextDraw::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDraw::SetPosition( const SPoint &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CTextDraw::GetText() const
{
	return wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetText @0x329ce0: nSize = 0 (force a regenerate); wsText = arg (self-assign guarded in
// retail); push the text into the layout object.
void CTextDraw::SetText( const wstring &_wsText )
{
	nSize = 0;
	if ( &_wsText != &wsText )
		wsText = _wsText;
	pML->SetText( wsText, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail Draw @0x329b10: resolve the size, map position+rect to screen (through the window, or the
// raw 1024x768 scale when free-standing), then let the layout render. NOTE retail's tail does NOT
// re-run UpdateText here (GetSize already did) -- the old dev tail call is gone.
void CTextDraw::Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView )
{
	SPoint sTextSize = GetSize( pView );
	SRect sScrWindow( sPosition.x, sPosition.y, sPosition.x + sTextSize.x, sPosition.y + sTextSize.y );
	SPoint sScrPosition( sPosition );
	if ( pWindow )
	{
		if ( !pWindow->ClientToScreen( &sScrPosition, &sScrWindow, false ) )
			return;
		pWindow->VirtualToScreen( &sScrPosition, &sScrWindow );
	}
	else
	{
		CVec2 vScreenRect = pView->GetViewportSize();

		float fXCoef = vScreenRect.x / 1024.0f;
		float fYCoef = vScreenRect.y / 768.0f;
		sScrPosition.x *= fXCoef;
		sScrPosition.y *= fYCoef;
		sScrWindow.x1 *= fXCoef;
		sScrWindow.y1 *= fYCoef;
		sScrWindow.x2 *= fXCoef;
		sScrWindow.y2 *= fYCoef;
	}

	pML->Render( pView, sScrPosition, sScrWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail UpdateText @0x329680: resolve the layout width in screen pixels (a positive requested
// width scaled by viewport/1024, else the raw viewport width) and regenerate the layout only when
// it changed since the cached nSize.
void CTextDraw::UpdateText( NGScene::I2DGameView *pView )
{
	CVec2 vScreenRect = pView->GetViewportSize();
	int nNewSize;
	if ( sSize.x > 0 )
		nNewSize = int( float( sSize.x ) * vScreenRect.x / 1024.0f );   // retail truncates here (or ah,0xc)
	else
		nNewSize = int( vScreenRect.x );

	if ( nNewSize != nSize )
	{
		nSize = nNewSize;
		pML->Generate( pView, nNewSize );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImageDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageDraw::CImageDraw( const SRect &_sWindow, NDb::CUITexture* _pTexture, const SRect &_sTexRect, const NGfx::SPixel8888 &_sColor ):
	sWindow( _sWindow ), pUITexture(_pTexture), sTextureRect( _sTexRect ), sColor( _sColor ), vScale( 1, 1 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SRect& CImageDraw::GetWindow()
{
	return sWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageDraw::SetWindow( const SRect &_sWindow )
{
	sWindow = _sWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageDraw::SetScale( const CVec2 &_vScale )
{
	vScale = _vScale;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageDraw::SetColor( const NGfx::SPixel8888 &_sColor )
{
	sColor = _sColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageDraw::SetImage( NDb::CUITexture* _pTexture, const SRect &sTexRect )
{
	pUITexture = _pTexture;
	sTextureRect = sTexRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageDraw::Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView )
{
	CVec2 vScreenRect = pView->GetViewportSize();
	NGfx::SPixel8888 sDrawColor;
	MakeColor( &sDrawColor, sColor );

	SRect sScrWindow( sWindow );
	SPoint sScrPosition( sWindow.x1, sWindow.y1 );
	if ( pWindow )
	{
		if ( !pWindow->ClientToScreen( &sScrPosition, &sScrWindow, false ) )
			return;
		pWindow->VirtualToScreen( &sScrPosition, &sScrWindow );
	}
	else
	{
		float fXCoef = vScreenRect.x / 1024.0f;
		float fYCoef = vScreenRect.y / 768.0f;
		sScrPosition.x *= fXCoef;
		sScrPosition.y *= fYCoef;
		sScrWindow.x1 *= fXCoef;
		sScrWindow.y1 *= fYCoef;
		sScrWindow.x2 *= fXCoef;
		sScrWindow.y2 *= fYCoef;
	}

	if ( IsValid( pUITexture ) )
	{
		NDb::EUIMode eMode;
		CTPoint<float> scale( 1.0f, 1.0f );
		switch( Float2Int( vScreenRect.x ) )
		{
			case 1600:
				if ( IsValid( pUITexture->pTextures[NDb::UIM_1600x1200] ) )
				{
					eMode = NDb::UIM_1600x1200;
					break;
				}
				scale.x *= 1600.0f / 1280.0f;
				scale.y *= 1200.0f / 1024.0f;
			case 1280:
				if ( IsValid( pUITexture->pTextures[NDb::UIM_1280x1024] ) )
				{
					eMode = NDb::UIM_1280x1024;
					break;
				}
				scale.x *= 1280.0f / 1024.0f;
				scale.y *= 1024.0f / 768.0f;
			case 1024:
				if ( IsValid( pUITexture->pTextures[NDb::UIM_1024x768] ) )
				{
					eMode = NDb::UIM_1024x768;
					break;
				}
				scale.x *= 1024.0f / 800.0f;
				scale.y *= 768.0f / 600.0f;
			case 800:
				if ( IsValid( pUITexture->pTextures[NDb::UIM_800x600] ) )
				{
					eMode = NDb::UIM_800x600;
					break;
				}
			default:
				if ( !IsValid( pUITexture->pTextures[NDb::UIM_1024x768] ) )
					return;

				eMode = NDb::UIM_1024x768;
				scale.x = vScreenRect.x / 1024.0f;
				scale.y = vScreenRect.y / 768.0f;
				break;
		}

		NDb::CTexture *pTexture = pUITexture->pTextures[eMode];
		ASSERT( ( pTexture->nWidth != 0 ) && ( pTexture->nHeight != 0 ) );
		if ( ( pTexture->nWidth == 0 ) || ( pTexture->nHeight == 0 ) )
			return;

		CTRect<float> sTexRect( sTextureRect.x1, sTextureRect.y1, sTextureRect.x2, sTextureRect.y2 );
		if ( ( sTexRect.Width() == 0 ) && ( sTexRect.Height() == 0 ) )
		{
			sTexRect.x1 = 0;
			sTexRect.x2 = pTexture->nWidth;
			sTexRect.y1 = pTexture->nHeight;
			sTexRect.y2 = 0;
		}

		// retail 12-byte CRectLayout has no scale member -- the tile quad size (|texrect| * the
		// mode-scale * the user vScale) is baked into every AddRect (retail 6-arg AddRect @0x174620).
		const CVec2 vTileScale( scale.x * vScale.x, scale.y * vScale.y );
		const float fTileSizeX = fabsf( sTexRect.Width() ) * vTileScale.x;
		const float fTileSizeY = fabsf( sTexRect.Height() ) * vTileScale.y;
		CRectLayout sLayout;
		for ( int nTempY = 0; nTempY < sWindow.Height(); nTempY += pUITexture->nHeight )
			for ( int nTempX = 0; nTempX < sWindow.Width(); nTempX += pUITexture->nWidth )
				sLayout.AddRect( nTempX * vTileScale.x, nTempY * vTileScale.y, fTileSizeX, fTileSizeY, sTexRect, sDrawColor );

		pView->CreateDynamicRects( pTexture, sLayout, sScrPosition, sScrWindow );
	}
	else
	{
		// texture-less fill: the old scale (vp/1024, vp/768) times the |texrect| (= the window dims)
		CRectLayout sLayout;
		sLayout.AddRect( 0, 0,
			sWindow.Width() * pView->GetViewportSize().x / 1024.0f,
			sWindow.Height() * pView->GetViewportSize().y / 768.0f,
			CTRect<float>( 0, 0, sWindow.Width(), sWindow.Height() ), sDrawColor );
		pView->CreateDynamicRects( (NDb::CTexture*)0, sLayout, sScrPosition, sScrWindow );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModelWrap
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x329710: Init + the fixed UI-item projection.
void MakeProjection( CTransformStack *pTS )
{
	pTS->Init();
	pTS->MakeProjective( CVec2( 1024, 768 ), 60, 0.1f, 300 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x329830 (disasm-verified consts 512/384): conjugate the widget-center NDC shift through
// the projection, then camera, then the model matrix; backward = identity.
void MakeModelTransform( SFBTransform *pRes, const SPoint &sPosition, const SPoint &sSize, const SHMatrix &sCameraTransform, const SHMatrix &sModelTransform )
{
	CTransformStack tsCamera;
	tsCamera.Init();
	tsCamera.SetCamera( sCameraTransform );

	CTransformStack tsProjection;
	MakeProjection( &tsProjection );

	CVec2 vCenter( sPosition.x + sSize.x / 2, sPosition.y + sSize.y / 2 );

	SHMatrix sShift;
	Identity( &sShift );
	sShift._14 = ( vCenter.x - 512.0f ) / 512.0f;
	sShift._24 = ( 384.0f - vCenter.y ) / 384.0f;

	SHMatrix sA, sB;
	Multiply( &sA, sShift, tsProjection.Get().forward );
	Multiply( &sB, tsProjection.Get().backward, sA );
	Multiply( &sA, sB, tsCamera.Get().forward );
	Multiply( &pRes->forward, sA, sModelTransform );
	Identity( &pRes->backward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x32a920: identity wire matrices, then SetScene(0,true) -> own fast view, bParentScene=false.
CModelDraw::CModelDraw( const SRect &_sWindow, NDb::CModel* _pModel ):
	sWindow( _sWindow ), pModel( _pModel ), sColor( 0xFF, 0xFF, 0xFF, 0xFF )
{
	CTransformStack ts;
	ts.Init();
	pTransform = new NGScene::CCFBTransform;
	pTransform->Set( ts.Get() );

	Identity( &sModelTransform );
	Identity( &sCameraTransform );

	SetScene( 0, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SRect& CModelDraw::GetWindow() const
{
	return sWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetWindow( const SRect &_sWindow )
{
	sWindow = _sWindow;
	// retail @0x329570: SetWindow ALWAYS raises bUpdated -- CModel::Draw (@0x3126f0) calls it every
	// frame, so the placement fold (MakeModelTransform) tracks the window's current screen position
	bUpdated = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const NGfx::SPixel8888& CModelDraw::GetColor() const
{
	return sColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetColor( const NGfx::SPixel8888 &_sColor )
{
	sColor = _sColor;
	CVec3 vAmbient( 0.5f * float( sColor.r ) / 0xFF, 0.5f * float( sColor.g ) / 0xFF, 0.5f * float( sColor.b ) / 0xFF );
	p3DView->SetAmbient( vAmbient, vAmbient );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CModel* CModelDraw::GetModel() const
{
	return pModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetModel( NDb::CModel* _pModel )
{
	pModel = _pModel;
	pRender = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x32a390. (bFast=false is retail CreateNewInterfaceView @0x18b3e0 -- new CGameView +
// SetInterfaceMode(1); dev's scene has no interface-mode knob, CreateNewView is its standing stand-in.)
void CModelDraw::SetScene( NGScene::IGameView *pView, bool bFast )
{
	pRender = 0;
	if ( IsValid( pView ) )
	{
		p3DView = pView;
		bParentScene = true;
	}
	else
	{
		bParentScene = false;
		p3DView = bFast ? NGScene::CreateNewFastInterfaceView() : NGScene::CreateNewView();

		SRand rnd;
		NDb::CTAmbientLight *pLight = NDb::GetTAmbientLight( 7 );
		if ( IsValid( pLight ) )
			p3DView->SetAmbient( pLight->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetModelTransform( const SHMatrix &sMatrix )
{
	bUpdated = true;
	sModelTransform = sMatrix;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetCameraTransform( const SHMatrix &sMatrix )
{
	bUpdated = true;
	sCameraTransform = sMatrix;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x32a4d0. Flow: off-screen -> DROP the mesh; lazy (re)create from the serialized
// pModel+pTransform; bUpdated -> fold sCameraTransform/sModelTransform + placement into pTransform
// (CCFBTransform::Set bumps the version, the mesh's transform functor picks it up); a parent-scene
// mesh is drawn BY the parent -- only an own-view CModelDraw draws p3DView here.
void CModelDraw::Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !pModel )
		return;

	SRect sScrWindow( sWindow );
	SPoint sScrPosition( sWindow.x1, sWindow.y1 );
	if ( pWindow && !pWindow->ClientToScreen( &sScrPosition, &sScrWindow, false ) )
	{
		pRender = 0;
		return;
	}

	if ( !IsValid( pRender ) )
		pRender = p3DView->CreateMesh( pModel, pTransform );

	if ( bUpdated )
	{
		bUpdated = false;
		SFBTransform sResult;
		MakeModelTransform( &sResult, sScrPosition, SPoint( sWindow.Width(), sWindow.Height() ), sCameraTransform, sModelTransform );
		pTransform->Set( sResult );
	}

	if ( bParentScene )
		return;

	SRect s2DScrWindow( sScrWindow );
	SPoint s2DScrPosition( sScrPosition );
	SPoint sRealSize( sWindow.Width(), sWindow.Height() );
	if ( pWindow )
	{
		pWindow->VirtualToScreen( &s2DScrPosition, &s2DScrWindow );
		pWindow->VirtualToScreen( &sRealSize, 0 );
	}
	else
	{
		const CVec2 &vScreenRect = pView->GetViewportSize();

		float fXCoef = vScreenRect.x / 1024.0f;
		float fYCoef = vScreenRect.y / 768.0f;
		sRealSize.x *= fXCoef;
		sRealSize.y *= fYCoef;
		s2DScrPosition.x *= fXCoef;
		s2DScrPosition.y *= fYCoef;
		s2DScrWindow.x1 *= fXCoef;
		s2DScrWindow.y1 *= fYCoef;
		s2DScrWindow.x2 *= fXCoef;
		s2DScrWindow.y2 *= fYCoef;
	}

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, sRealSize.x, sRealSize.y, CTRect<float>( 0, 0, 0, 0 ) );
	pView->CreateDynamicClearRects( sLayout, s2DScrPosition, s2DScrWindow, 1.0f );
	pView->Flush();

	CTransformStack ts;
	MakeProjection( &ts );

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &ts;
	drawInfo.vOrigin = CVec2( sScrWindow.x1 / 1024.0f, sScrWindow.y1 / 768.0f );
	drawInfo.vSize = CVec2( sScrWindow.Width() / 1024.0f, sScrWindow.Height() / 768.0f );
	drawInfo.bOverlay = true;
	p3DView->Draw( drawInfo );

	pView->CreateDynamicClearRects( sLayout, s2DScrPosition, s2DScrWindow, 0.0f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0814160, CTextDraw )
REGISTER_SAVELOAD_CLASS( 0xB0814161, CImageDraw )
REGISTER_SAVELOAD_CLASS( 0xB0814163, CModelDraw )
