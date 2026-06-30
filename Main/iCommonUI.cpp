#include "StdAfx.h"
#include "GSceneUtils.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "GView.h"
#include "G2DView.h"
#include "wInterface.h"
#include "RPGMerc.h"
#include "RPGUnit.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataCamera.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "UIWrap.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "RWGame.h"
#include "iGameStates.h"
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalcFlashCoeff( float fCoeff, float fTargetCoeff, const STime &sTime, const STime &sFlashTime, const STime &sMorphTime )
{
	float fDelta = float( sTime - sFlashTime ) / sMorphTime;
	if ( fCoeff > fTargetCoeff )
	{
		fCoeff -= fDelta;
		if ( fCoeff < fTargetCoeff )
			fCoeff = fTargetCoeff;
	}
	else if ( fCoeff < fTargetCoeff )
	{
		fCoeff += fDelta;
		if ( fCoeff > fTargetCoeff )
			fCoeff = fTargetCoeff;
	}

	return fCoeff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLineBar
////////////////////////////////////////////////////////////////////////////////////////////////////
CLineBar::CLineBar( const SWindowInfo &sInfo ):
	CImage( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLineBar::Set( float fBar )
{
	SetSize( SPoint( nBarWidth * fBar, GetSize().y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLineBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATECREATE:
		{
			nBarWidth = GetSize().x;
			break;
		}
	}

	return CImage::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImageNumber
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageNumber::CImageNumber( const SWindowInfo &sInfo, EType eType ):
	CWindow( sInfo ), textureIDs( 10 ), nValue( -1 ), sColor( 0xFF, 0xFF, 0xFF, 0xFF )
{
	switch( eType )
	{
	case TYPE_UNITINFOPANEL:
		{
			textureIDs[0] = 369;
			textureIDs[1] = 370;
			textureIDs[2] = 371;
			textureIDs[3] = 372;
			textureIDs[4] = 373;
			textureIDs[5] = 374;
			textureIDs[6] = 375;
			textureIDs[7] = 376;
			textureIDs[8] = 377;
			textureIDs[9] = 378;
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageNumber::Set( int _nValue )
{
	if ( nValue == _nValue )
		return;

	nValue = _nValue;
	imagesList.clear();

	if ( nValue < 0 )
		return;

	sRealSize.x = 0;
	sRealSize.y = 0;
	int nX = GetSize().x;
	int nTemp = nValue;
	do
	{
		int nNumber = nTemp % 10;
		nTemp = nTemp / 10;

		CPtr<NDb::CUITexture> pTexture = NDb::GetUITexture( textureIDs[nNumber] );
		CPtr<CImage> pImage = new CImage( SWindowInfo( this, SPoint( nX - pTexture->nWidth, 0 ), SPoint( pTexture->nWidth, pTexture->nHeight ), "number", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );
		pImage->SetColor( sColor );
		pImage->SetImage( pTexture );
		imagesList.push_back( pImage.GetPtr() );

		nX -= pTexture->nWidth;
		sRealSize.x += pTexture->nWidth;
		sRealSize.y = Max( sRealSize.y, pTexture->nHeight );
	}	while( nTemp > 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageNumber::SetColor( const NGfx::SPixel8888 &_sColor )
{
	sColor = _sColor;

	for ( list<CObj<CImage> >::iterator iTemp = imagesList.begin(); iTemp != imagesList.end(); iTemp++ )
		(*iTemp)->SetColor( sColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CImageNumber::GetRealSize() const
{
	return sRealSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImageNumber::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_LBUTTONDOWN:
			return true;
		case EVENT_LBUTTONUP:
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CShrinkButton::CShrinkButton( const SWindowInfo &sInfo ):
	CButton( sInfo ), bChecked( false )
{
	statesSet.resize( STATE_MAXVALUE );
	for ( int nTemp = 0; nTemp < STATE_MAXVALUE; nTemp++ )
		statesSet[nTemp] = AddState( nTemp );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShrinkButton::IsChecked() const
{
	return bChecked;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShrinkButton::SetChecked( bool bState )
{
	bChecked = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CImage* CShrinkButton::AddImageToState( EState eState, NDb::CUITexture *pTexture, const NGfx::SPixel8888 &sColor, const CVec2 &vScale )
{
	CWindow *pState = statesSet[eState];

	SPoint sSize( pState->GetSize() );
	sSize.x *= vScale.x;
	sSize.y *= vScale.y;
	SPoint sShift( ( pState->GetSize().x - sSize.x ) / 2, ( pState->GetSize().y - sSize.y ) / 2 );

	CImage* pImage = new CImage( SWindowInfo( pState, sShift, sSize, "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );
	pImage->SetScale( vScale );
	pImage->SetColor( sColor );
	pImage->SetImage( pTexture );
	return pImage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShrinkButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( GetStyle( STYLE_ENABLED ) )
	{
		if ( !IsPushed() )
			SetActiveState( !bChecked ? STATE_NORMAL_UP : STATE_CHECKED_UP );
		else
			SetActiveState( !bChecked ? STATE_NORMAL_DOWN : STATE_CHECKED_DOWN );
	}
	else
		SetActiveState( STATE_DISABLED );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CComplexButton::CComplexButton( const SWindowInfo &sInfo, NDb::CUITexture *pUp, NDb::CUITexture *pDown, NDb::CUITexture *pUnchecked, NDb::CUITexture *pChecked ):
	CShrinkButton( sInfo )
{
	pDisabled = AddImageToState( STATE_DISABLED, 0, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );

	pNormalUpCheck = AddImageToState( STATE_NORMAL_UP, pUnchecked, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pNormalUpIcon = AddImageToState( STATE_NORMAL_UP, 0, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pNormalUp = AddImageToState( STATE_NORMAL_UP, pUp, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pNormalDownCheck = AddImageToState( STATE_NORMAL_DOWN, pUnchecked, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );
	pNormalDownIcon = AddImageToState( STATE_NORMAL_DOWN, 0, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );
	pNormalDown = AddImageToState( STATE_NORMAL_DOWN, pDown, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );

	pNormalUpCheck->SetStyle( STYLE_VISIBLE, IsValid( pUnchecked ) );
	pNormalUp->SetStyle( STYLE_VISIBLE, IsValid( pUp ) );
	pNormalDownCheck->SetStyle( STYLE_VISIBLE, IsValid( pUnchecked ) );
	pNormalDown->SetStyle( STYLE_VISIBLE, IsValid( pDown ) );

	pCheckedUpCheck = AddImageToState( STATE_CHECKED_UP, pChecked, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pCheckedUpIcon = AddImageToState( STATE_CHECKED_UP, 0, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pCheckedUp = AddImageToState( STATE_CHECKED_UP, pUp, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 1, 1 ) );
	pCheckedDownCheck = AddImageToState( STATE_CHECKED_DOWN, pChecked, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );
	pCheckedDownIcon = AddImageToState( STATE_CHECKED_DOWN, 0, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );
	pCheckedDown = AddImageToState( STATE_CHECKED_DOWN, pDown, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ), CVec2( 0.92f, 0.92f ) );

	pCheckedUpCheck->SetStyle( STYLE_VISIBLE, IsValid( pChecked ) );
	pCheckedUp->SetStyle( STYLE_VISIBLE, IsValid( pUp ) );
	pCheckedDownCheck->SetStyle( STYLE_VISIBLE, IsValid( pChecked ) );
	pCheckedDown->SetStyle( STYLE_VISIBLE, IsValid( pDown ) );

	pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CToolTip* CComplexButton::GetToolTip() const
{
	return pToolTip;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComplexButton::Set( NDb::CUITexture *pIcon, NDb::CUITexture *pIconDisabled, EState eState, const string &szID )
{
	pDisabled->SetImage( pIconDisabled );

	pNormalUpIcon->SetImage( pIcon );
	pNormalDownIcon->SetImage( pIcon );

	pCheckedUpIcon->SetImage( pIcon );
	pCheckedDownIcon->SetImage( pIcon );

	pNormalUpCheck->SetStyle( STYLE_VISIBLE, eState != NORMAL );
	pNormalDownCheck->SetStyle( STYLE_VISIBLE, eState != NORMAL );

	SetChecked( eState == CHECKED );
	SetNotifyID( szID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComplexButton::SetColor( const NGfx::SPixel8888 &sColor )
{
	pNormalUp->SetColor( sColor );
	pNormalDown->SetColor( sColor );
	pNormalUpIcon->SetColor( sColor );
	pNormalDownIcon->SetColor( sColor );

	pCheckedUp->SetColor( sColor );
	pCheckedDown->SetColor( sColor );
	pCheckedUpIcon->SetColor( sColor );
	pCheckedDownIcon->SetColor( sColor );

	CButton::SetColor( sColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CHoverButton::CHoverButton( const SWindowInfo &sInfo ):
	CButton( sInfo ), bForceState( false ), nStateID( -1 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoverButton::ForceState( bool _bForceState, int _nStateID )
{
	nStateID = _nStateID;
	bForceState = _bForceState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoverButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !bForceState )
	{
		if ( GetStyle( STYLE_ENABLED ) )
		{
			if ( IsMouseCover() )
				SetActiveState( STATE_HOVER );
			else
				SetActiveState( STATE_NORMAL );
		}
		else
			SetActiveState( STATE_DISABLED );
	}
	else
		SetActiveState( nStateID );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFlashButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CFlashButton::CFlashButton( const SWindowInfo &sInfo, NDb::CUITexture *_pBackgroundTexture, NDb::CUITexture *_pActiveTexture ):
	CButton( sInfo ), pBackgroundTexture( _pBackgroundTexture ), pActiveTexture( _pActiveTexture ), fCoeff( 0 ), sMorphTime( 0 ), bFlashMode( true )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFlashButton::GetFlashMode() const
{
	return bFlashMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashButton::SetFlashMode( bool bState )
{
	bFlashMode = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFlashButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pActive = GetUIWindow<CImage>( this, "active" );
			if ( IsValid( pActiveTexture ) )
				pActive->SetImage( pActiveTexture );

			pBackground = GetUIWindow<CImage>( this, "background" );
			if ( IsValid( pBackgroundTexture ) )
				pBackground->SetImage( pBackgroundTexture );

			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashButton::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !GetStyle( STYLE_ENABLED ) )
	{
		pActive->SetStyle( STYLE_VISIBLE, false );
		return;
	}

	pActive->SetStyle( STYLE_VISIBLE, true );

	float fTargetCoeff = 1.0f;
	if ( !IsMouseCover() && bFlashMode )
	{
		fTargetCoeff = float( sTime % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
		if ( fTargetCoeff > 1 )
			fTargetCoeff = 2 - fTargetCoeff;
	}

	fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
	sMorphTime = sTime;

	pActive->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );

	CButton::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverFlashButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CHoverFlashButton::CHoverFlashButton( const SWindowInfo &sInfo ):
	CHoverButton( sInfo )
{
	pImage = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 952 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoverFlashButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CHoverButton::Draw( sTime, pView );

	if ( GetStyle( STYLE_ENABLED ) )
	{
		float fTargetCoeff = ( IsMouseCover() && bShowFlash ) ? 1.0f : 0.0f;
		if ( !IsMouseCover() && bShowFlash )
		{
			fTargetCoeff = float( sTime % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
			if ( fTargetCoeff > 1 )
				fTargetCoeff = 2 - fTargetCoeff;
		}

		fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
		sMorphTime = sTime;

		pImage->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
		pImage->Draw( this, sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScrollWindowBase::CScrollWindowBase( const SWindowInfo &sInfo ): 
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CScrollWindowBase::GetClient() const
{
	return pClient;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::SetClient( CWindow *_pClient )
{
	pClient = _pClient;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2& CScrollWindowBase::GetValue() const
{
	return vValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::SetValue( const CVec2 &_vValue )
{
	vValue = _vValue;

	if ( IsValid( pHScroll ) )
		pHScroll->SetValue( vValue.x * pHScroll->GetMaxValue() );
	if ( IsValid( pVScroll ) )
		pVScroll->SetValue( vValue.y * pVScroll->GetMaxValue() );

	UpdateScrollers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScroll* CScrollWindowBase::GetHScroll() const
{
	return pHScroll;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::SetHScroll( CScroll *pScroll )
{
	pHScroll = pScroll;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScroll* CScrollWindowBase::GetVScroll() const
{
	return pVScroll;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::SetVScroll( CScroll *pScroll )
{
	pVScroll = pScroll;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScrollWindowBase::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_SCROLL:
		{
			CVec2 vValue = GetValue();
			vValue.y += sEvent.fVal;
			SetValue( vValue );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pClient ) ) 
	{
		UpdateScrollers();

		const SPoint &sViewSize = GetSize();
		const SPoint &sClientSize = pClient->GetSize();

		SPoint sDelta( Max( 0, sClientSize.x - sViewSize.x ), Max( 0, sClientSize.y - sViewSize.y ) );
		sDelta.x = -sDelta.x * vValue.x;
		sDelta.y = -sDelta.y * vValue.y;

		pClient->SetPosition( sDelta );

		if ( IsValid( pHScroll ) )
			pHScroll->SetStyle( STYLE_VISIBLE, ( sClientSize.x > sViewSize.x ) );
		if ( IsValid( pVScroll ) )
			pVScroll->SetStyle( STYLE_VISIBLE, ( sClientSize.y > sViewSize.y ) );
	}

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScrollWindowBase::UpdateScrollers()
{
	if ( IsValid( pHScroll ) )
		vValue.x = float( pHScroll->GetValue() ) / pHScroll->GetMaxValue();
	if ( IsValid( pVScroll ) )
		vValue.y = float( pVScroll->GetValue() ) / pVScroll->GetMaxValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitHead
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitHead::CUnitHead( const SWindowInfo &sInfo, NRender::IRenderGame *_pRender, float _fScale ):
	CWindow( sInfo ), pRenderGame( _pRender ), fScale( _fScale )
{
	CTransformStack ts;
	ts.Init();
	//ts.Push( CQuat( -FP_PI2, CVec3(0,0,1) ) );
	pTransform = new NGScene::CCFBTransform;
	pTransform->Set( ts.Get() );

	p3DView = NGScene::CreateNewFastInterfaceView();
	NDb::CTAmbientLight *p = NDb::GetTAmbientLight(7);
	SRand rnd;
	p3DView->SetAmbient( p->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitHead::SetUnit( NWorld::CUnit *pUnit )
{
	pHead = NRender::CreateShowUnitHead( p3DView, pUnit, pRenderGame->GetHeadController(), pTransform );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitHead::SetSequence( NDb::CSequence *pSequence )
{
	ASSERT( IsValid( pHead ) );
	if ( !IsValid( pHead ) )
		return;

	pHead->SetSequence( pSequence );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitHead::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pHead ) )
		return;

	SPoint sSize( (float)GetSize().x * p3DView->GetScreenRect().x / 1024.0f, (float)GetSize().y * p3DView->GetScreenRect().y / 768.0f );

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	SRect s2DWindow( sWindow );
	SPoint s2DPosition( sPosition );
	VirtualToScreen( &s2DPosition, &s2DWindow );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, s2DWindow.Width(), s2DWindow.Height() ) );
	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 1.0f );
	pView->Flush();

	CVec2 vPos( sPosition.x + sWindow.Width() / 2, sPosition.y + sWindow.Height() / 2 );
	CTransformStack ts;
	SHMatrix sMatrix;
	MakeMatrix( &sMatrix, CVec3( 0, -5 / fScale, 0.048f ), QNULL, CVec3( 1, 1, 1 ) );
	ts.Init();
	ts.MakeProjective( CVec2( 1024, 768 ), 60, 0.1f, 1000 );
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
	drawInfo.vOrigin = CVec2( sPosition.x / 1024.0f, sPosition.y / 768.0f );
	drawInfo.vSize = CVec2( sWindow.Width() / 1024.0f, sWindow.Height() / 768.0f );
	drawInfo.bOverlay = true;
	p3DView->Draw( drawInfo );

	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 0.0f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitView::CUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *_pRenderGame, float _fScale ):
	CWindow( sInfo ), pRenderGame( _pRenderGame ), fScale( _fScale )
{
	p3DView = NGScene::CreateNewFastInterfaceView();

	SetLight( NDb::GetTAmbientLight( 7 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::SetUnit( NRPG::CUnit *pUnit, ECameraType eType )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );

	NDb::SCameraParams sCameraParams;
	switch( eType )
	{
	case CAMERA_FACEGEN:
		sCameraParams = pUnit->GetPers()->sFaceGenCamera;
		break;
	case CAMERA_PORTRAIT:
		sCameraParams = pUnit->GetPers()->sPortraitCamera;
		break;
	default:
		ASSERT( 0 );
		sCameraParams = pUnit->GetPers()->sPortraitCamera;
		break;
	}

	fFOV = 60; //sCameraParams.fFOV;
	fYaw = sCameraParams.fYaw;
	fPitch = sCameraParams.fPitch;
	fDistance = sCameraParams.fDistance;
	vAnchor = sCameraParams.vAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::SetUnit( NWorld::CUnit *pUnit, ECameraType eType )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );

	NDb::SCameraParams sCameraParams;
	switch( eType )
	{
	case CAMERA_FACEGEN:
		sCameraParams = pUnit->GetRPG()->GetRPGPers()->sFaceGenCamera;
		break;
	case CAMERA_PORTRAIT:
		sCameraParams = pUnit->GetRPG()->GetRPGPers()->sPortraitCamera;
		break;
	default:
		ASSERT( 0 );
		sCameraParams = pUnit->GetRPG()->GetRPGPers()->sPortraitCamera;
		break;
	}

	fFOV = 60; //sCameraParams.fFOV;
	fYaw = sCameraParams.fYaw;
	fPitch = sCameraParams.fPitch;
	fDistance = sCameraParams.fDistance;
	vAnchor = sCameraParams.vAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::SetUnit( NWorld::CUnit *pUnit, NDb::CDBCamera *pCamera )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) || !IsValid( pCamera ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );

	fFOV = 60; //sCameraParams.fFOV;
	fYaw = pCamera->fYaw;
	fPitch = pCamera->fPitch;
	fDistance = pCamera->fDistance;
	vAnchor = pCamera->vAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CUnitView::SetUnit @0x1c0310: the NRPG::CUnit* global-camera variant FaceGen calls. Identical to the
// NWorld::CUnit* overload above except the unit type handed to CreateShowUnit; reads the global CDBCamera
// (fFOV=60; yaw/pitch/distance/anchor from the record) rather than the pers camera.
void CUnitView::SetUnit( NRPG::CUnit *pUnit, NDb::CDBCamera *pCamera )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) || !IsValid( pCamera ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );

	fFOV = 60; //sCameraParams.fFOV;
	fYaw = pCamera->fYaw;
	fPitch = pCamera->fPitch;
	fDistance = pCamera->fDistance;
	vAnchor = pCamera->vAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::SetLight( NDb::CTAmbientLight *pLight )
{
	if ( !IsValid( pLight ) )
		return;

	SRand rnd;
	p3DView->SetAmbient( pLight->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::SetSequence( NDb::CSequence *pSequence )
{
	ASSERT( IsValid( pInventoryUnit ) );
	if ( !IsValid( pInventoryUnit ) )
		return;

	pInventoryUnit->SetSequence( pSequence );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::PlayAnimation( NDb::CAnimation *pAnim, bool bLoop )
{
	if ( IsValid( pInventoryUnit ) )
		pInventoryUnit->PlayAnimation( pAnim, bLoop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CWindow::Draw( sTime, pView );

	if ( !IsValid( pInventoryUnit ) )
		return;

	SPoint sSize( (float)GetSize().x * p3DView->GetScreenRect().x / 1024.0f, (float)GetSize().y * p3DView->GetScreenRect().y / 768.0f );

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	SRect s2DWindow( sWindow );
	SPoint s2DPosition( sPosition );
	VirtualToScreen( &s2DPosition, &s2DWindow );

	SRect sDummyWindow( sWindow );
	SPoint sRealSize( GetSize() );
	VirtualToScreen( &sRealSize, &sDummyWindow );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sRealSize.x, sRealSize.y ) );
	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 1.0f );
	pView->Flush();

	sTimer.Advance( true, GetTickCount() );
	pInventoryUnit->Update( 0 );

	CVec2 vPos( sPosition.x + GetSize().x / 2, sPosition.y + GetSize().y / 2 );

	CVec3 vForwardDir;
	CQuat q = CQuat( fYaw, V3_AXIS_Z ) * CQuat( fPitch, V3_AXIS_X );
	q.GetYAxis( &vForwardDir );

	CVec3 vCP( vAnchor - vForwardDir * fDistance );
	SHMatrix sCamera;
	MakeMatrix( &sCamera, fPitch, fYaw, 0, vCP );

	CTransformStack ts;
	ts.Init();
	ts.MakeProjective( CVec2( 1024, 768 ), fFOV * fScale, 0.1f, 300 );
	ts.SetCamera( sCamera );

	SHMatrix sShift;
	Identity( &sShift);
	sShift._14 = (float)( vPos.x - 512 ) / 512;
	sShift._24 = (float)( 384 - vPos.y ) / 384;

	SHMatrix sRes;
	Multiply( &sRes, sShift, ts.Get().forward );
	ts.Init( sRes );

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &ts;
	drawInfo.vOrigin = CVec2( sPosition.x / 1024.0f, sPosition.y / 768.0f );
	drawInfo.vSize = CVec2( sWindow.Width() / 1024.0f, sWindow.Height() / 768.0f );
	drawInfo.bOverlay = true;
	p3DView->Draw( drawInfo );

	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 0.0f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CInteractiveUnitView::CInteractiveUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *_pRender ):
	CWindow( sInfo ), pRenderGame( _pRender ), bButtonDown( false ), fAngle( 0.0f ), sLastPoint( 0, 0 )
{
	p3DView = NGScene::CreateNewFastInterfaceView();

	SRand rnd;
	NDb::CTAmbientLight *p = NDb::GetTAmbientLight(7);
	p3DView->SetAmbient( p->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInteractiveUnitView::SetUnit( NRPG::CUnit *pUnit )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInteractiveUnitView::SetUnit( NWorld::CUnit *pUnit )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release: AdvFaceGen binds the unit view with a global DataCamera (5014) for a PERSPECTIVE head closeup
// (the CUnitView::SetUnit @0x1c0310 idiom). The camera params switch Draw from the orthographic full-body view
// (CharGen/inventory) to the perspective head view; fAngle still rotates the model interactively.
void CInteractiveUnitView::SetUnit( NRPG::CUnit *pUnit, NDb::CDBCamera *pCamera )
{
	pInventoryUnit = 0;
	bHasCamera = false;
	if ( !IsValid( pUnit ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame );

	if ( IsValid( pCamera ) )
	{
		fFOV = pCamera->fFOV;   // DataCamera 5014's own FOV (35) -- tighter/closer than CUnitView's hardcoded 60
		fYaw = pCamera->fYaw;
		fPitch = pCamera->fPitch;
		fDistance = pCamera->fDistance;
		vAnchor = pCamera->vAnchor;
		bHasCamera = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInteractiveUnitView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );

			if ( !bButtonDown )
				break;

			fAngle += float( sEvent.nX - sLastPoint.x ) / 10;
			sLastPoint.x = sEvent.nX;
			sLastPoint.y = sEvent.nY;
			break;
		}
	case EVENT_LBUTTONUP:
		{
			bButtonDown = false;
			pMouseCapture = 0;
			return true;
		}
	case EVENT_LBUTTONDOWN:
		{
			bButtonDown = true;
			sLastPoint.x = sEvent.nX;
			sLastPoint.y = sEvent.nY;
			pMouseCapture = GetInterface()->CreateMouseCapture( this );
			return true;
		}
	case EVENT_MOUSECAPTURELOSE:
		{
			bButtonDown = false;
			pMouseCapture = 0;
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInteractiveUnitView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pInventoryUnit ) )
		return;

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	SRect s2DWindow( sWindow );
	SPoint s2DPosition( sPosition );
	VirtualToScreen( &s2DPosition, &s2DWindow );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, s2DWindow.Width(), s2DWindow.Height() ) );
	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 1.0f );
	pView->Flush();

	sTimer.Advance( true, GetTickCount() );
	pInventoryUnit->Update( fAngle );

	CTransformStack ts;
	ts.Init();
	if ( bHasCamera )
	{
		// Perspective head view from the DataCamera (AdvFaceGen, 5014), rotated interactively by fAngle
		// (mirror of CUnitView::Draw).
		CVec2 vPos( sPosition.x + GetSize().x / 2, sPosition.y + GetSize().y / 2 );
		CVec3 vForwardDir;
		CQuat q = CQuat( fYaw, V3_AXIS_Z ) * CQuat( fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );
		CVec3 vCP( vAnchor - vForwardDir * fDistance );
		SHMatrix sCamera;
		MakeMatrix( &sCamera, fPitch, fYaw, 0, vCP );
		ts.MakeProjective( CVec2( 1024, 768 ), fFOV, 0.1f, 300 );
		ts.SetCamera( sCamera );
		SHMatrix sShift;
		Identity( &sShift );
		sShift._14 = (float)( vPos.x - 512 ) / 512;
		sShift._24 = (float)( 384 - vPos.y ) / 384;
		SHMatrix sRes;
		Multiply( &sRes, sShift, ts.Get().forward );
		ts.Init( sRes );
	}
	else
	{
		// Orthographic full-body view (CharGen + the inventory model show) -- unchanged.
		CVec2 vPos( ( sWindow.x2 + sWindow.x1 ) / 2, ( sWindow.y2 + sWindow.y1 ) / 2 + 190 );
		SHMatrix sMatrix;
		MakeMatrix( &sMatrix, ToRadian( 0 ), ToRadian( 90.0f ), 0, CVec3( 10, (float)( 512 - vPos.x ) * 5 / 1024, (float)( vPos.y - 384 ) * 3.75f / 768 ) );
		ts.MakeParallel( 5, 3.75f, 0, 20 );
		ts.SetCamera( sMatrix );
	}

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &ts;
	drawInfo.vOrigin = CVec2( sPosition.x / 1024.0f, sPosition.y / 768.0f );
	drawInfo.vSize = CVec2( sWindow.Width() / 1024.0f, sWindow.Height() / 768.0f );
	drawInfo.bOverlay = true;
	p3DView->Draw( drawInfo );

	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 0.0f );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemModel
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowItemModel::CShowItemModel( const SWindowInfo &sInfo ):
	CModel( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IInventoryItem* CShowItemModel::Get() const
{
	return pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowItemModel::Set( NRPG::IInventoryItem* _pItem, NDb::ECameraType eCameraType )
{
	pItem = _pItem;

	CPtr<NDb::CRPGItem> pRPGItem( pItem->GetDBItem() );

	SRand sRnd;
	if ( pRPGItem->pModel )
	{
		const NDb::SCameraParams &sCamera = pRPGItem->sCameras[eCameraType];

		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SFBTransform res;
		MakeMatrix( &res, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		SetModel( pRPGItem->pModel->CreateModel( &sRnd ) );
		SetTransform( new CFBTransform( res ) );
	}

	pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowItemModel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pItem ) )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	CPtr<NDb::CRPGItem> pRPGItem( pItem->GetDBItem() );

	pToolTip->SetVal( L"name", GetDBString( pRPGItem->pName ) );
	pToolTip->SetVal( L"weight", pItem->GetWeight() );
	CDynamicCast<NRPG::IWeaponItemInfo> pWeapon(pItem);
	if (pWeapon)
	{
		pToolTip->SetText( GetDBString( 4468 ) );

		CPtr<NDb::CRPGWeapon> pRPGWeapon( pWeapon->GetDBWeapon() );
		CPtr<NRPG::IClipItem> pClipItem( pWeapon->GetInnerClip() );

		pToolTip->SetVal( L"typename", GetDBString( pRPGWeapon->pWeaponType->pName ) );

		struct SShootMode
		{
			int nStringID;
			WCHAR* pszID;
		};
		SShootMode pShotModeNames[NDb::SM_MAXVALUE] =
		{
			6068, L"snapshot",
			6069, L"aimedshot",
			6070, L"carefulshot",
			6071, L"shortburst",
			6072, L"longburst",
			6073, L"snipe",
		};
		for ( int nTemp = 0; nTemp < NDb::SM_MAXVALUE; nTemp++ )
		{
			if ( !pRPGWeapon->shootModes[nTemp] )
				continue;

			WCHAR wsBuffer[256];
			swprintf( wsBuffer, L"%s: --- ", GetDBString( pShotModeNames[nTemp].nStringID ) );
			pToolTip->SetVal( pShotModeNames[nTemp].pszID, wsBuffer );
		}

		NRPG::SWeaponInfo sInfo;
		pWeapon->GetInfo( &sInfo );

		pToolTip->SetVal( L"range", sInfo.nMaxRange );
		pToolTip->SetVal( L"damagemin", sInfo.nDmgMin );
		pToolTip->SetVal( L"damagemax", sInfo.nDmgMax );

		pToolTip->SetVal( L"ammotype", GetDBString( pClipItem->GetDBAmmo()->pName ) );
		pToolTip->SetVal( L"clipsize", pClipItem->GetMaxIncQuantity() );

		pToolTip->SetVal( L"convamateur", sInfo.nMinRange );
		pToolTip->SetVal( L"convprofessional", sInfo.nMaxRange );

		if ( pRPGWeapon->shootModes[NDb::SM_ShortBurst] )
		{
			WCHAR wsBuffer[256];
			swprintf( wsBuffer, L"<br>Short Burst: %d shots Stability: %d", sInfo.nRoF, sInfo.nRecoil );
			pToolTip->SetVal( L"burstinfo", sInfo.nMaxRange );
		}
	}
	else {
		CDynamicCast<NRPG::IGrenadeItemInfo> pClip(pItem);
		if (pClip)
		{
			pToolTip->SetText(GetDBString(4469));

			CPtr<NDb::CRPGGrenade> pRPGGrenade(pClip->GetDBGrenade());

			pToolTip->SetVal(L"typename", GetDBString(pRPGGrenade->pWeaponType->pName));
			pToolTip->SetVal(L"delay", pRPGGrenade->nMaxDelay);
		}
		else {
			CDynamicCast<NRPG::IMeleeWeaponItem> pMeleeWeapon(pItem);
			if (pMeleeWeapon)
			{
				pToolTip->SetText(GetDBString(4512));

				CPtr<NDb::CRPGMeleeWeapon> pRPGMeleeWeapon(pMeleeWeapon->GetDBMeleeWeapon());

				pToolTip->SetVal(L"typename", GetDBString(pRPGMeleeWeapon->pWeaponType->pName));
				pToolTip->SetVal(L"damagemin", pRPGMeleeWeapon->nDmgMin);
				pToolTip->SetVal(L"damagemax", pRPGMeleeWeapon->nDmgMax);
				pToolTip->SetVal(L"critbonus", pRPGMeleeWeapon->nCriticalBonus);
			}
			else {
				CDynamicCast<NRPG::IClipItem> pClip(pItem);
				if (pClip)
				{
					pToolTip->SetText(GetDBString(4470));

					CPtr<NDb::CRPGAmmo> pRPGAmmo(pClip->GetDBAmmo());

					pToolTip->SetVal(L"ammotype", GetDBString(pRPGAmmo->pName));
					pToolTip->SetVal(L"ammomax", pClip->GetMaxIncQuantity());
					pToolTip->SetVal(L"currammo", pClip->GetIncQuantity());
					pToolTip->SetVal(L"damagemin", pRPGAmmo->nDmgMin);
					pToolTip->SetVal(L"damagemax", pRPGAmmo->nDmgMax);
				}
				else {
					CDynamicCast<NRPG::IFirstAidItem> pFirstAid(pItem);
					if (pFirstAid)
						pToolTip->SetText(GetDBString(4511));
					else
						pToolTip->SetText(GetDBString(4690));
				}
			}
		}
	}

	CModel::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemModel
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemModel::CItemModel( const SWindowInfo &sInfo, NGame::IMission *pMission ):
	TBaseClass( sInfo, pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemModel::CanHandleState( NGame::IState *pState ) const
{
	CDynamicCast<NGame::CStateUnloadItem> pItem(pState);
	if (pItem)
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CItemModel::GetTarget()
{
	return Get();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlot::CSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission, int _nWidth, int _nHeight, NDb::ECameraType _eCameraType, bool _bAlwaysHilight ):
	CWindow( sInfo ), pMission( _pMission ), nWidth( _nWidth ), nHeight( _nHeight ), eCameraType( _eCameraType ), bAlwaysHilight( _bAlwaysHilight ), bTrackMouse( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlot::SetSize( const SPoint &sSize )
{
	CWindow::SetSize( sSize );
	if ( IsValid( pHilight ) )
		pHilight->SetSize( sSize );
	if ( IsValid( pSlotView ) )
		pSlotView->SetSize( sSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlot::SetSlotSize( int _nWidth, int _nHeight )
{
	nWidth = _nWidth;
	nHeight = _nHeight;

	hilights.SetSizes( nWidth, nHeight );
	const SPoint &sSize = pSlotView->GetSize();
	for ( int nTempY = 0; nTempY < nHeight; nTempY++ )
	{
		for ( int nTempX = 0; nTempX < nWidth; nTempX++ )
		{
			SPoint sPoint( sSize.x * nTempX, sSize.y * nTempY );
			hilights[nTempY][nTempX].pImage = new CImage( SWindowInfo( pHilight, SPoint( sPoint.x / nWidth, sPoint.y / nHeight ), SPoint( sSize.x / nWidth , sSize.y / nHeight ), "", STYLE_ENABLED | STYLE_TRANSPARENT ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlot::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			sMousePoint = SPoint( sEvent.nX, sEvent.nY );
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_MOUSEENTER:
		{
			bTrackMouse = true;
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			bTrackMouse = false;
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pHilight = GetUIWindow<CWindow>( this, "hilight" );
			pSlotView = GetUIWindow<CWindow>( this, "view" );
			SetSlotSize( nWidth, nHeight );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	const SPoint &sSize = pSlotView->GetSize();
	SPoint sCellSize( sSize.x / nWidth, sSize.y / nHeight );

	vector<SItem> newInventoryItemsSet;
	GetItemsList( &newInventoryItemsSet );

	vector<SItem> newItemsSet( newInventoryItemsSet.size() );
	for ( int nTemp = 0; nTemp < newInventoryItemsSet.size(); nTemp++ )
	{
		const SItem &sItem = newInventoryItemsSet[nTemp];
		for ( int nItem = 0; nItem < itemsSet.size(); nItem++ )
		{
			const SItem &sTestItem = itemsSet[nItem];
			if ( ( sTestItem.sPos != sItem.sPos ) || ( sTestItem.pItem != sItem.pItem ) )
				continue;

			newItemsSet[nTemp] = itemsSet[nItem];
			break;
		}

		if ( !IsValid( newItemsSet[nTemp].pItem ) )
		{
			newItemsSet[nTemp].sPos = sItem.sPos;
			newItemsSet[nTemp].pItem = sItem.pItem;

			const SPoint &sInventoryItemSize = sItem.pItem->GetSize();
			SPoint sItemCellSize( sCellSize.x * Min( sInventoryItemSize.x, nWidth ), sCellSize.y * Min( sInventoryItemSize.y, nHeight ) );
			SPoint sItemSize( sItemCellSize.x - 1, sItemCellSize.y - 1 );

			CPtr<NDb::CRPGItem> pRPGItem( sItem.pItem->GetDBItem() );
			SPoint sShift( 1 + ( sItemCellSize.x - sItemSize.x ) / 2, 1 + ( sItemCellSize.y - sItemSize.y ) / 2 );
			SPoint sPosition( sItem.sPos.x * sCellSize.x + sShift.x, sItem.sPos.y * sCellSize.y + sShift.y );
			CPtr<CItemModel> pItemModel = new CItemModel( SWindowInfo( pSlotView, sPosition, sItemSize, "icon", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ), pMission );
			pItemModel->Set( sItem.pItem, eCameraType );
			newItemsSet[nTemp].pModel = pItemModel;
//			newItemsSet[nTemp].pImage = IImage::Create( pView, SRect( sPosition.x, sPosition.y, sPosition.x + sItemSize.x, sPosition.y + sItemSize.y ), "icon", STYLE_ENABLED | STYLE_VISIBLE );
//			newItemsSet[nTemp].pImage->SetImage( pTexture );
//			newItemsSet[nTemp].pImage->SetColor( NGfx::SPixel8888( 0xFF, 0, 0, 0xFF ) );

		}
	}

	itemsSet = newItemsSet;

	for ( int nTempY = 0; nTempY < nHeight; nTempY++ )
	{
		for ( int nTempX = 0; nTempX < nWidth; nTempX++ )
		{
			SHilight &sHilight = hilights[nTempY][nTempX];

			sHilight.nID = -1;
			sHilight.pImage->SetStyle( STYLE_VISIBLE, false );
		}
	}

	for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
	{
		const SItem &sItem = itemsSet[nTemp];
		const SPoint &sPos = sItem.sPos;
		SPoint sSize = sItem.pItem->GetSize();
		sSize = SPoint( min( nWidth, sPos.x + sSize.x ) - sPos.x, min( nHeight, sPos.y + sSize.y ) - sPos.y );

		for ( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		{
			for ( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			{
				SHilight &sHilight = hilights[sPos.y + nTempY][sPos.x + nTempX];
				sHilight.nID = nTemp;
				sHilight.pImage->SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0xFF, 0x2F ) );
				sHilight.pImage->SetStyle( STYLE_VISIBLE, bAlwaysHilight );
			}
		}
	}

	NWorld::IPlayer::SItemInfo sItemInfo;
	if ( bTrackMouse && GetDragItem( &sItemInfo ) )
	{
		SPoint sSize = sItemInfo.pItem->GetSize();
		SPoint sPos;
		GetItemInSlotPos( sMousePoint.x, sMousePoint.y, sSize, &sPos );
		sSize = SPoint( min( nWidth, sPos.x + sSize.x ) - sPos.x, min( nHeight, sPos.y + sSize.y ) - sPos.y );

		bool bReplace = false;
		bool bCanPlace = CanPlace( sMousePoint.x, sMousePoint.y, NWorld::SItem( sItemInfo.pUnit, NWorld::SItem::HAND, sItemInfo.pItem ) );
		if ( bCanPlace )
		{
			int nLastID = -1;
			for ( int nTempY = 0; nTempY < sSize.y; nTempY++ )
			{
				for ( int nTempX = 0; nTempX < sSize.x; nTempX++ )
				{
					SHilight &sHilight = hilights[sPos.y + nTempY][sPos.x + nTempX];
					if ( sHilight.nID == -1 )
						continue;

					if ( ( nLastID != -1 ) && ( sHilight.nID != nLastID ) )
					{
						bCanPlace = false;
						break;
					}

					nLastID = sHilight.nID;
					if ( sHilight.nID != -1 )
						bReplace = true;
				}
			}
		}

		for ( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		{
			for ( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			{
				SHilight &sHilight = hilights[sPos.y + nTempY][sPos.x + nTempX];

				if ( !bCanPlace )
				{
					sHilight.pImage->SetColor( NGfx::SPixel8888( 0xFF, 0x1F, 0x1F, 0x2F ) );
					sHilight.pImage->SetStyle( STYLE_VISIBLE, true );
					continue;
				}

				if ( bReplace )
				{
					sHilight.pImage->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0x2F ) );
					sHilight.pImage->SetStyle( STYLE_VISIBLE, true );
				}
				else
				{
					sHilight.pImage->SetColor( NGfx::SPixel8888( 0x1F, 0xFF, 0x1F, 0x2F ) );
					sHilight.pImage->SetStyle( STYLE_VISIBLE, true );
				}
			}
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlot::GetInSlotPos( int nX, int nY, SPoint *pCoords )
{
	SRect sWindow;
	SPoint sPosition;
	ClientToScreen( &sPosition, &sWindow );

	const SPoint &sSize = pSlotView->GetSize();
	SPoint sCellSize( sSize.x / nWidth, sSize.y / nHeight );

	SPoint sLocalSpaceCursor( nX - sPosition.x, nY - sPosition.y );

	pCoords->x = Min( nWidth, Max( 0, sLocalSpaceCursor.x / sCellSize.x ) );
	pCoords->y = Min( nHeight, Max( 0, sLocalSpaceCursor.y / sCellSize.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlot::GetItemInSlotPos( int nX, int nY, const SPoint &sItemSize, SPoint *pCoords )
{
	SRect sWindow;
	SPoint sPosition;
	ClientToScreen( &sPosition, &sWindow );

	const SPoint &sSize = pSlotView->GetSize();
	SPoint sCellSize( sSize.x / nWidth, sSize.y / nHeight );

	SPoint sLocalSpaceCursor( nX - sPosition.x, nY - sPosition.y );

	pCoords->x = Min( nWidth - Min( nWidth, sItemSize.x ), Max( 0, Float2Int( ( float( sLocalSpaceCursor.x ) - float( sItemSize.x * sCellSize.x ) / 2 ) / sCellSize.x ) ) );
	pCoords->y = Min( nHeight - Min( nHeight, sItemSize.y ), Max( 0, Float2Int( ( float( sLocalSpaceCursor.y ) - float( sItemSize.y * sCellSize.y ) / 2 ) / sCellSize.y ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlot::GetDragItem( NWorld::IPlayer::SItemInfo *pInfo )
{
	return pMission->GetActivePlayer()->GetPlayer()->GetInHandItem( pInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::IMission* CSlot::GetGame()
{
	return pMission;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CButtonsLine
////////////////////////////////////////////////////////////////////////////////////////////////////
// Transient per-state record built while a button's text states are created (the line collects them
// only to centre each state's text inside the button, then drops the vector). NOT serialized.
struct SButtonsLineReflow
{
	CPtr<CWindow> pState;
	CPtr<CMLText> pText;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Create one text state on a button and size both the state and the button to fit the caption.
// Mirrors NUI::AddDynamicState @0x1c5500: a CMLText (markup text, so it measures its own natural
// size via GetRealSize) is placed in the new state window; the state is sized to text+8 x lineHeight,
// and the button grows to fit its widest caption. (The binary's exact button-width formula is
// Ghidra-ambiguous; fit-to-widest-caption is the observable behaviour -- buttons sized to their text.)
static void AddDynamicState( CButton *pButton, int nState, const wstring &wsText, int nLineHeight, vector<SButtonsLineReflow> *pReflow )
{
	SButtonsLineReflow sEntry;
	sEntry.pState = pButton->AddState( nState );
	CMLText *pText = new CMLText( SWindowInfo( sEntry.pState, SPoint( 0, 0 ), SPoint( 1024, nLineHeight ), "text" ) );
	sEntry.pText = pText;
	pText->SetText( wsText, true );

	SPoint sReal;
	pText->GetRealSize( &sReal );
	sReal.x += 8;
	sEntry.pState->SetSize( SPoint( sReal.x, nLineHeight ) );
	pButton->SetSize( SPoint( Max( sReal.x, pButton->GetSize().x ), nLineHeight ) );

	pReflow->push_back( sEntry );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Centre each state's text horizontally inside the button (NUI::ReflowButtonText @0x1bed70).
static void ReflowButtonText( CButton *pButton, vector<SButtonsLineReflow> *pReflow )
{
	int nButtonWidth = pButton->GetSize().x;
	for ( int nTemp = 0; nTemp < pReflow->size(); ++nTemp )
	{
		CWindow *pState = (*pReflow)[nTemp].pState;
		if ( !IsValid( pState ) )
			continue;
		int nStateWidth = pState->GetSize().x;
		pState->SetPosition( SPoint( ( nButtonWidth - nStateWidth ) / 2, 0 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Distribute the buttons evenly across the line, justified to both edges (NUI::ReflowButtons @0x1bee00):
// the free width (line - sum of button widths) is split into equal gaps between buttons.
static void ReflowButtons( CWindow *pLine, vector<CObj<CButton> > *pButtons )
{
	int nCount = pButtons->size();
	float fFree = float( pLine->GetSize().x );
	for ( int nTemp = 0; nTemp < nCount; ++nTemp )
		fFree -= (*pButtons)[nTemp]->GetSize().x;

	float fGap = ( nCount > 1 ) ? fFree / ( nCount - 1 ) : 0.0f;
	float fPos = 0.0f;
	for ( int nTemp = 0; nTemp < nCount; ++nTemp )
	{
		(*pButtons)[nTemp]->SetPosition( SPoint( int( fPos + 0.5f ), 0 ) );
		fPos += (*pButtons)[nTemp]->GetSize().x + fGap;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CButtonsLine::CButtonsLine( const SWindowInfo &sInfo ):
	CWindow( sInfo ), bUpdated( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButtonsLine::AddButton( CButton *pButton, int nTooltipID, const wstring &wsNormal, const wstring &wsHover, const wstring &wsState3, const wstring &wsDisabled )
{
	bUpdated = true;

	int nLineHeight = GetSize().y;
	vector<SButtonsLineReflow> reflow;
	AddDynamicState( pButton, CHoverButton::STATE_HOVER,    wsHover,    nLineHeight, &reflow );	// state 1
	AddDynamicState( pButton, CHoverButton::STATE_NORMAL,   wsNormal,   nLineHeight, &reflow );	// state 0
	AddDynamicState( pButton, 3,                            wsState3,   nLineHeight, &reflow );	// state 3
	AddDynamicState( pButton, CHoverButton::STATE_DISABLED, wsDisabled, nLineHeight, &reflow );	// state 2
	ReflowButtonText( pButton, &reflow );

	if ( nTooltipID != -1 )
	{
		CPtr<CToolTip> pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
		pToolTip->SetText( GetDBString( nTooltipID ) );
		pButton->SetToolTip( pToolTip );
	}

	buttonsSet.push_back( pButton );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CHoverButton* CButtonsLine::AddHoverButton( const string &szID, int nTooltipID, const wstring &wsNormal, const wstring &wsHover, const wstring &wsDisabled )
{
	CHoverButton *pButton = new CHoverButton( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), szID, STYLE_ENABLED | STYLE_VISIBLE ) );
	AddButton( pButton, nTooltipID, wsNormal, wsHover, L"", wsDisabled );
	return pButton;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButtonsLine::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( bUpdated )
	{
		bUpdated = false;
		ReflowButtons( this, &buttonsSet );
	}
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverCheckButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CHoverCheckButton::CHoverCheckButton( const SWindowInfo &sInfo ):
	CHoverButton( sInfo ), bChecked( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoverCheckButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// While checked, force state 3 (the selected/down art) -- release Draw @0x1bdff0.
	ForceState( bChecked, 3 );
	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoverCheckButton::OnAction()
{
	// Toggle the check, then run the base action -- release OnAction @0x1be010.
	bChecked = !bChecked;
	CHoverButton::OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB2243951, CLineBar );
REGISTER_SAVELOAD_CLASS( 0xB2243952, CUnitHead );
REGISTER_SAVELOAD_CLASS( 0xB2243953, CImageNumber );
REGISTER_SAVELOAD_CLASS( 0xB2243954, CFlashButton );
REGISTER_SAVELOAD_CLASS( 0xB2243955, CHoverButton );
REGISTER_SAVELOAD_CLASS( 0xB3529130, CHoverCheckButton );
REGISTER_SAVELOAD_CLASS( 0xB3708180, CHoverFlashButton );
REGISTER_SAVELOAD_CLASS( 0xB2243956, CUnitView );
REGISTER_SAVELOAD_CLASS( 0xB2243957, CScrollWindowBase );
REGISTER_SAVELOAD_CLASS( 0xB2243958, CShowItemModel );
REGISTER_SAVELOAD_CLASS( 0xB2243959, CItemModel );
REGISTER_SAVELOAD_CLASS( 0xB224395A, CShrinkButton );
REGISTER_SAVELOAD_CLASS( 0xB224395B, CComplexButton );
REGISTER_SAVELOAD_CLASS( 0xB3620190, CComplexButtonFlash );
REGISTER_SAVELOAD_CLASS( 0xB224395C, CInteractiveUnitView );
REGISTER_SAVELOAD_CLASS( 0xb3601001, CButtonsLine );
