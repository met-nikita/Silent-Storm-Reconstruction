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
#include "DiscretePos.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
CTextDraw::CTextDraw( const SPoint &_sPosition, const SPoint &_sSize, const wstring &_wsText ):
	sPosition( _sPosition ), sSize( _sSize ), sRealSize( _sSize ), wsText(_wsText)
{
	pSize = new NGScene::CCTPoint;
	pTextString = new NGScene::CCWString;

	pSize->Set( sSize );
	pTextString->Set( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CTextDraw::GetSize( NGScene::I2DGameView *pView )
{
	if ( IsValid( pView ) && ( ( sSize.x == -1 ) || ( sSize.y == -1 ) ) )
	{
		UpdateSize( pView );

		if ( !pText )
			pText = pView->CreateText( pTextString, pSize );

		pText.Refresh();
		const CVec2 &vScreenRect = pView->GetViewportSize();
		const NGScene::SText &sText = pText->GetValue();

		sRealSize = sSize;
		if ( sSize.x == -1 )
			sRealSize.x = sText.sSize.x * 1024 / vScreenRect.x;
		if ( sSize.y == -1 )
			sRealSize.y = sText.sSize.y * 768 / vScreenRect.y;

		return sRealSize;
	}

	return sSize;
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
void CTextDraw::SetText( const wstring &_wsText )
{
	wsText = _wsText;
	pTextString->Set( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDraw::Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !pText )
		pText = pView->CreateText( pTextString, pSize );

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

	UpdateSize( pView );

	pView->CreateDynamicRects( pText, sScrPosition, sScrWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDraw::UpdateSize( NGScene::I2DGameView *pView )
{
	CVec2 vScreenRect = pView->GetViewportSize();
	SPoint sSetSize( sSize );
	if ( sSize.x != -1 )
		sSetSize.x = sSize.x * vScreenRect.x / 1024;
	if ( sSize.y != -1 )
		sSetSize.y = sSize.y * vScreenRect.y / 768;

	pSize.Refresh();
	if ( pSize->GetValue() != sSetSize )
		pSize->Set( sSetSize );
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

		CRectLayout sLayout;
		sLayout.scale.x = scale.x * vScale.x;
		sLayout.scale.y = scale.y * vScale.y;
		for ( int nTempY = 0; nTempY < sWindow.Height(); nTempY += pUITexture->nHeight )
			for ( int nTempX = 0; nTempX < sWindow.Width(); nTempX += pUITexture->nWidth )
				sLayout.AddRect( nTempX * sLayout.scale.x, nTempY * sLayout.scale.y, sTexRect, sColor );

		pView->CreateDynamicRects( pTexture, sLayout, sScrPosition, sScrWindow );
	}
	else
	{
		CRectLayout sLayout;
		sLayout.scale.x = pView->GetViewportSize().x / 1024.0f;
		sLayout.scale.y = pView->GetViewportSize().y / 768.0f;
		sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sWindow.Width(), sWindow.Height() ), sColor );
		pView->CreateDynamicRects( (NDb::CTexture*)0, sLayout, sScrPosition, sScrWindow );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModelWrap
////////////////////////////////////////////////////////////////////////////////////////////////////
CModelDraw::CModelDraw( const SRect &_sWindow, NDb::CModel* _pModel, CFBTransform *_pBaseTransform ):
	sWindow( _sWindow ), pModel( _pModel ), pBaseTransform( _pBaseTransform ), sColor( 0xFF, 0xFF, 0xFF, 0xFF )
{
	CTransformStack ts;
	ts.Init();
	pTransform = new NGScene::CCFBTransform;
	pTransform->Set( ts.Get() );

	p3DView = NGScene::CreateNewFastInterfaceView();

	SRand rnd;
	NDb::CTAmbientLight *p = NDb::GetTAmbientLight(7);
	p3DView->SetAmbient( p->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
/*
	p3DView = NGScene::CreateNewFastInterfaceView();//CreateNewView();
	p3DView->SetAmbient( 0 );
	pAmbientDirectional = p3DView->AddDirectionalLight( CVec3(0.5f,0.4f,0.45f), CVec3( 0.6f,1.4f,-1), CVec3(5,5,0), CVec2( 150, 150 ), 20 );
	p3DView->SetAmbient( CVec3( 0.5f, 0.5f, 0.5f ) );
*/
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
CFBTransform* CModelDraw::GetTransform() const
{
	return pBaseTransform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModelDraw::SetTransform( CFBTransform *_pBaseTransform )
{
	pBaseTransform = _pBaseTransform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// iSpecialView (release) setters @0x32a390 / @0x3295b0 / @0x3295d0 over the existing fields.
////////////////////////////////////////////////////////////////////////////////////////////////////
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
void CModelDraw::Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !pModel )
		return;

	SRect sScrWindow( sWindow );
	SPoint sScrPosition( sWindow.x1, sWindow.y1 );
	if ( pWindow )
	{
		if ( !pWindow->ClientToScreen( &sScrPosition, &sScrWindow, false ) )
			return;
	}

	SPoint sSize( (float)sWindow.Width() * p3DView->GetScreenRect().x / 1024.0f, (float)sWindow.Height() * p3DView->GetScreenRect().y / 768.0f );
	SRect s2DScrWindow( sScrWindow );
	SPoint s2DScrPosition( sScrPosition );
	if ( pWindow )
		pWindow->VirtualToScreen( &s2DScrPosition, &s2DScrWindow );
	else
	{
		CVec2 vScreenRect = pView->GetViewportSize();

		float fXCoef = vScreenRect.x / 1024.0f;
		float fYCoef = vScreenRect.y / 768.0f;
		s2DScrPosition.x *= fXCoef;
		s2DScrPosition.y *= fYCoef;
		s2DScrWindow.x1 *= fXCoef;
		s2DScrWindow.y1 *= fYCoef;
		s2DScrWindow.x2 *= fXCoef;
		s2DScrWindow.y2 *= fYCoef;
	}

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, s2DScrWindow.Width(), s2DScrWindow.Height() ) );
	pView->CreateDynamicClearRects( sLayout, s2DScrPosition, s2DScrWindow, 1.0f );
	pView->Flush();

	if ( !IsValid( pRender ) )
		pRender = p3DView->CreateMesh( pModel, pTransform );

	CVec2 vPos( sScrPosition.x + sWindow.Width() / 2, sScrPosition.y + sWindow.Height() / 2 );
	CTransformStack ts;
	SHMatrix sMatrix;
	if ( pBaseTransform )
		sMatrix = pBaseTransform->pos.forward;
//	MakeMatrix( &sMatrix, ToRadian( 0 ), ToRadian( 90.0f ), CVec3( 10, (float)( 512 - vPos.x ) * 5 / 1024, (float)( vPos.y - 384 ) * 3.75f / 768 ) );
	ts.Init();
	ts.MakeProjective( CVec2( 1024, 768 ), 60, 0.1f, 300 );
	ts.SetCamera( sMatrix );

	SHMatrix sShift;
	Identity( &sShift);
	sShift._14 = (float)( vPos.x - 512 ) / 512;
	sShift._24 = (float)( 384 - vPos.y ) / 384;

	SHMatrix sRes;
	Multiply( &sRes, sShift, ts.Get().forward );
	ts.Init( sRes );

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &ts;
	drawInfo.vOrigin = CVec2( sScrPosition.x / 1024.0f, sScrPosition.y / 768.0f );
	drawInfo.vSize = CVec2( sWindow.Width() / 1024.0f, sWindow.Height() / 768.0f );
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