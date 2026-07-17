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
#include "RPGItem.h"   // GetGrenadeRec* which-record accessors
#include "RPGItemSet.h"             // CClipItem (drag-compat yellow) -- CSlot::Draw @0x1c34d0 tint pass
#include "..\DBFormat\DataMisc.h"   // NDb::CRPGPicklock (the can't-use red predicate)
#include "..\DBFormat\DataPerk.h"   // NDb::CDBPerk (tool/picklock pNeededPerk id)
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
// CUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitView::CUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *_pRenderGame, float _fScale ):
	CWindow( sInfo ), pRenderGame( _pRenderGame ), fScale( _fScale ), fFOV( 60 ),
	fAngle( 0 ), fYaw( 0 ), fPitch( 0 ), fDistance( 0 ), vAnchor( 0, 0, 0 )
{
	Identity( &sCamera );
	p3DView = NGScene::CreateNewFastInterfaceView();

	SetLight( NDb::GetTAmbientLight( 7 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Compose the serialized camera transform (sCamera) from the runtime orbit params. Mirrors the old
// inline Draw math (MakeMatrix from yaw/pitch and the anchor-offset camera point); called by every
// SetUnit so sCamera stays the single source Draw reads (and the one that round-trips through a save).
void CUnitView::RecalcCamera()
{
	CVec3 vForwardDir;
	CQuat q = CQuat( fYaw, V3_AXIS_Z ) * CQuat( fPitch, V3_AXIS_X );
	q.GetYAxis( &vForwardDir );
	CVec3 vCP( vAnchor - vForwardDir * fDistance );
	MakeMatrix( &sCamera, fPitch, fYaw, 0, vCP );
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
	RecalcCamera();
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
	RecalcCamera();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x1c03d0: SetUnit(unit, camera, bItems, bShowCap, bPlayIdle). The three bools are threaded into
// NRender::CreateShowUnit in the retail push order (@0x1c043c..3e): (bItems, bPlayIdle, bShowCap).
void CUnitView::SetUnit( NWorld::CUnit *pUnit, NDb::CDBCamera *pCamera, bool bItems, bool bShowCap, bool bPlayIdle )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) || !IsValid( pCamera ) )
		return;

	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, sTimer.GetTime(), pRenderGame, bItems, bPlayIdle, bShowCap );

	fFOV = 60; //sCameraParams.fFOV;
	fYaw = pCamera->fYaw;
	fPitch = pCamera->fPitch;
	fDistance = pCamera->fDistance;
	vAnchor = pCamera->vAnchor;
	RecalcCamera();
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
	RecalcCamera();
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
void CUnitView::SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression )
{
	// release @0x1bf030: forward BOTH sequences (lipsync + per-phrase facial expression)
	ASSERT( IsValid( pInventoryUnit ) );
	if ( !IsValid( pInventoryUnit ) )
		return;

	pInventoryUnit->SetSequence( pSequence, pExpression );
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
	// retail @0x1bf070: with a live unit the 2D window draws LAST -- clear-rect(1.0) ->
	// Flush -> 3D head -> clear-rect(0.0) -> CWindow::Draw -- so the face background/frame
	// children land after the 3D depth punch-through instead of being wiped by it.
	if ( !IsValid( pInventoryUnit ) )
	{
		CWindow::Draw( sTime, pView );
		return;
	}

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
	sLayout.AddRect( 0, 0, sRealSize.x, sRealSize.y, CTRect<float>( 0, 0, sRealSize.x, sRealSize.y ) );
	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, 1.0f );
	pView->Flush();

	sTimer.Advance( true, GetTickCount() );
	pInventoryUnit->Update( fAngle );   // retail @0x1bf070 passes this->fAngle (tag 4; 0 for non-spinning hosts)

	CVec2 vPos( sPosition.x + GetSize().x / 2, sPosition.y + GetSize().y / 2 );

	CTransformStack ts;
	ts.Init();
	ts.MakeProjective( CVec2( 1024, 768 ), fFOV * fScale, 0.1f, 300 );
	// retail @0x1bf070: SetCamera( sCamera ) directly -- sCamera is the serialized camera transform,
	// composed from the orbit params by RecalcCamera() (so it round-trips through a save and is valid
	// on the first post-load frame before any SetUnit re-runs).
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

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1c04c0: delegates to the CUnitView base ctor (which builds p3DView and applies the
// GetTAmbientLight(7) inventory ambient -- exactly what this ctor's body used to do by hand) and
// clears the interaction state. (Retail's base ctor takes an extra trailing bool; dev's converged
// 3-arg CUnitView ctor covers the same setup.)
CInteractiveUnitView::CInteractiveUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *_pRender ):
	CUnitView( sInfo, _pRender ), bCapture( false ), bButtonDown( false ), fAngle( 0.0f ), sLastPoint( 0, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail has NO CInteractiveUnitView::SetUnit overloads (PDB) -- its callers pass the FULL-BODY
// global DataCamera 5013 "InventoryCamera" themselves (CharGen disasm @0x5b55f2 `mov ecx,0x1395`,
// same record as the inventory doll @0x5ee02e). These dev delegates keep the callers unchanged
// while composing the same retail camera into the serialized sCamera the perspective Draw reads.
void CInteractiveUnitView::SetUnit( NRPG::CUnit *pUnit )
{
	CUnitView::SetUnit( pUnit, NDb::GetDBCamera( 5013 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInteractiveUnitView::SetUnit( NWorld::CUnit *pUnit )
{
	CUnitView::SetUnit( pUnit, NDb::GetDBCamera( 5013 ), true, true, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release: AdvFaceGen binds the unit view with a global DataCamera (5014) for a PERSPECTIVE head
// closeup (the CUnitView::SetUnit @0x1c0310 idiom); composes sCamera via RecalcCamera.
void CInteractiveUnitView::SetUnit( NRPG::CUnit *pUnit, NDb::CDBCamera *pCamera )
{
	pInventoryUnit = 0;
	if ( !IsValid( pUnit ) )
		return;

	// CUnitView::sTimer -- the clock CUnitView::Draw advances (retail @0x1c0310 reads the BASE sTimer;
	// the derived shadow sTimer is wire-layout only, never Advanced -> a dead clock freezes every
	// CSkeletonAnimator in the doll graph at its bind-time pose).
	pInventoryUnit = NRender::CreateShowUnit( p3DView, pUnit, CUnitView::sTimer.GetTime(), pRenderGame );

	if ( IsValid( pCamera ) )
	{
		fFOV = pCamera->fFOV;   // DataCamera 5014's own FOV (35) -- tighter/closer than CUnitView's hardcoded 60
		fYaw = pCamera->fYaw;
		fPitch = pCamera->fPitch;
		fDistance = pCamera->fDistance;
		vAnchor = pCamera->vAnchor;
		RecalcCamera();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1c0550: LBUTTONDOWN only ARMS the capture (bCapture + mouse capture); the spin proper
// (bButtonDown) engages once the captured mouse strays more than 5px horizontally from the press
// point, and each further move integrates 0.05 rad per pixel (dev previously span immediately on
// button-down at 0.1/px and never serialized/used bCapture).
bool CInteractiveUnitView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );

			const int nDX = sEvent.nX - sLastPoint.x;
			if ( bCapture && ( nDX > 5 || nDX < -5 ) )
				bButtonDown = true;

			if ( bButtonDown )
			{
				const int nOldX = sLastPoint.x;
				sLastPoint.x = sEvent.nX;
				sLastPoint.y = sEvent.nY;
				fAngle += float( sEvent.nX - nOldX ) * 0.05f;
			}
			break;
		}
	case EVENT_LBUTTONUP:
		{
			bCapture = false;
			bButtonDown = false;
			pMouseCapture = 0;
			return true;
		}
	case EVENT_LBUTTONDOWN:
		{
			bCapture = true;
			sLastPoint.x = sEvent.nX;
			sLastPoint.y = sEvent.nY;
			pMouseCapture = GetInterface()->CreateMouseCapture( this );
			return true;
		}
	case EVENT_MOUSECAPTURELOSE:
		{
			bCapture = false;
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
	// retail @0x1bf390: copy the interactive spin into the base fAngle and delegate -- the doll
	// renders through the SAME perspective path as every unit view (@0x1bf070: MakeProjective +
	// the serialized sCamera). [The old dev-only ORTHO branch broke retail's HSR machinery:
	// GetCoverRect's perspective math is undefined on an ortho w-row, so any view running with
	// HSR_FAST (bFastMode=0 on the wire -- every RETAIL-save unit view) culled all doll parts.]
	CUnitView::fAngle = fAngle;
	CUnitView::Draw( sTime, pView );
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
// retail NUI::MakeGrenadeToolTip @0x1bf3b0: the shared static block for a blast record (thrown grenade,
// explosive bullet, mine's pExplosion) -- typename + the wave/fragment numbers + a 3-way weight line.
static void MakeGrenadeToolTip( CToolTip *pToolTip, NDb::CRPGGrenade *pG )
{
	pToolTip->SetVal( L"typename", GetDBString( pG->pWeaponType->pName ) );
	pToolTip->SetVal( L"explrange", Float2Int( pG->fWaveRadius ) );        // +0x2c
	pToolTip->SetVal( L"delay", pG->nMaxDelay );                          // +0x60
	pToolTip->SetVal( L"fragnum", pG->nFragmentNumber );                  // +0x30
	pToolTip->SetVal( L"fragmin", pG->nFragmentDmgMin );                  // +0x38
	pToolTip->SetVal( L"fragmax", pG->nFragmentDmgMax );                  // +0x3c
	pToolTip->SetVal( L"damagemin", Float2Int( pG->fWaveDmgMin ) );       // +0x18
	pToolTip->SetVal( L"damagemax", Float2Int( pG->fWaveDmgMax ) );       // +0x1c
	if ( IsValid( pG->pItem ) )
	{
		int nW = pG->pItem->nWeight;
		pToolTip->SetVal( L"weight", GetDBString( nW < 400 ? 17134 : ( nW < 1000 ? 17135 : 17136 ) ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowItemModel::Set( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, NRPG::IInventoryItem *_pItem, NDb::ECameraType eCameraType )
{
	pItem = _pItem;
	pUnit = _pUnit;

	// retail @0x1c0f50 calls GetItemInfo unconditionally -- no null-item guard exists there.
	CPtr<NDb::CRPGItem> pRPGItem( pItem->GetDBItem() );

	SRand sRnd;
	if ( pRPGItem->pModel )
	{
		const NDb::SCameraParams &sCamera = pRPGItem->sCameras[eCameraType];

		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SHMatrix sCameraTransform;
		MakeMatrix( &sCameraTransform, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		SetScene( pView, true );
		SetModel( pRPGItem->pModel->CreateModel( &sRnd ) );
		SetCameraTransform( sCameraTransform );
	}

	pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED ) );
	SetToolTip( pToolTip );

	// -------- the FULL STATIC tooltip build (retail CShowItemModel::Set @0x1c0f50) --------
	// The Jan03 build did this in Draw with a different key set; retail moved every fixed value here and
	// left only the live ammo/quantity/familiarity numbers to Draw. Key strings + string-DB ids verified
	// from the disasm; the game.db tooltip templates key on THESE names.
	pToolTip->SetVal( L"name", GetDBString( pRPGItem->pName ) );

	CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( pItem );
	CDynamicCast<NRPG::IGrenadeItemInfo> pGrenade( pItem );
	CDynamicCast<NRPG::IMeleeWeaponItem> pMelee( pItem );
	CDynamicCast<NRPG::IClipItem> pClip( pItem );
	CDynamicCast<NRPG::IFirstAidItem> pFirstAid( pItem );
	CDynamicCast<NRPG::IToolItem> pTool( pItem );
	CDynamicCast<NRPG::IPicklockItem> pPicklock( pItem );
	CDynamicCast<NRPG::IMineItem> pMine( pItem );
	CDynamicCast<NRPG::IKeyItem> pKey( pItem );
	CDynamicCast<NRPG::IClueItem> pClue( pItem );

	if ( pWeapon && IsValid( pWeapon->GetDBWeapon()->pWeaponType ) )
	{
		NDb::CRPGWeapon *pW = pWeapon->GetDBWeapon();
		NRPG::IClipItem *pInner = pWeapon->GetInnerClip();
		pToolTip->SetText( GetDBString( 4468 ) );
		pToolTip->SetVal( L"typename", GetDBString( pW->pWeaponType->pName ) );

		NRPG::SWeaponInfo sInfo;
		pWeapon->GetInfo( &sInfo );

		// shoot-mode rows: only the AVAILABLE modes print; the first printed row has no prefix, then
		// even printed-count -> "<br>", odd -> "<tab>" (retail @0x5c0f50 alternation).
		struct SShootMode { int nLabelID; const WCHAR *pszKey; };
		static const SShootMode kModes[NDb::SM_MAXVALUE] =
		{
			{ 6068, L"snapshot" }, { 6069, L"aimedshot" }, { 6070, L"carefulshot" },
			{ 6071, L"shortburst" }, { 6072, L"longburst" }, { 6073, L"snipe" },
		};
		int nPrinted = 0;
		for ( int i = 0; i < NDb::SM_MAXVALUE; ++i )
		{
			if ( !pW->shootModes[i] )
				continue;
			wstring wsSuffix;
			WCHAR wsBuf[128];
			switch ( i )
			{
			case NDb::SM_Snap:       swprintf( wsBuf, L"%s%d", GetDBString( 20710 ).c_str(), sInfo.nShotAP ); wsSuffix = wsBuf; break;
			case NDb::SM_Aimed:      swprintf( wsBuf, L"%s%d", GetDBString( 20710 ).c_str(), sInfo.nShotAP + sInfo.nTargetingAP ); wsSuffix = wsBuf; break;
			case NDb::SM_ShortBurst: swprintf( wsBuf, L"%s%d", GetDBString( 20710 ).c_str(), sInfo.nShotAP + sInfo.nRoF / 3 ); wsSuffix = wsBuf; break;
			case NDb::SM_Careful:
			case NDb::SM_LongBurst:  wsSuffix = GetDBString( 17856 ); break;
			case NDb::SM_Snipe:      wsSuffix = GetDBString( 19810 ); break;
			}
			const WCHAR *pszPrefix = ( nPrinted == 0 ) ? L"" : ( ( nPrinted & 1 ) ? L"<tab>" : L"<br>" );
			pToolTip->SetVal( kModes[i].pszKey, wstring( pszPrefix ) + GetDBString( kModes[i].nLabelID ) + wsSuffix + L" " );
			++nPrinted;
		}

		if ( pW->nRoF / 6 != 0 )
		{
			WCHAR wsBuf[128];
			swprintf( wsBuf, L"%s%d", GetDBString( 20711 ).c_str(), pW->nRoF / 6 );
			pToolTip->SetVal( L"brof", wsBuf );
		}
		if ( IsValid( pInner ) )
		{
			pToolTip->SetVal( L"ammotype", GetDBString( pInner->GetDBAmmo()->pName ) );
			pToolTip->SetVal( L"clipsize", pInner->GetMaxIncQuantity() );
		}
		// minrange 3-way vs the weapon type's handling ranges
		int nMinRangeID = ( sInfo.nMinRange > pW->pWeaponType->nHandlingEasyRange ) ? 20250
			: ( ( sInfo.nMinRange > pW->pWeaponType->nHandlingMediumRange ) ? 20251 : 20252 );
		pToolTip->SetVal( L"minrange", GetDBString( nMinRangeID ) );
		if ( pW->bScope )
			pToolTip->SetVal( L"issniper", GetDBString( 17130 ) );
		if ( pW->fSilencer < 1.0f )
			pToolTip->SetVal( L"issilenced", GetDBString( 17131 ) );
		if ( pW->shootModes[NDb::SM_ShortBurst] )
			pToolTip->SetVal( L"burstinfo", sInfo.nMaxRange );
	}
	else if ( pGrenade )
	{
		NDb::CRPGGrenade *pG = pGrenade->GetDBGrenade();
		if ( IsValid( pG ) )
		{
			pToolTip->SetText( GetDBString( pG->nFragmentNumber < 20 ? 17039 : 4469 ) );
			MakeGrenadeToolTip( pToolTip, pG );
		}
		else
		{
			NDb::CRPGEngGrenade *pE = pGrenade->GetDBEngGrenade();
			pToolTip->SetText( GetDBString( 20256 ) );
			if ( IsValid( pE ) )
			{
				pToolTip->SetVal( L"typename", GetDBString( pE->pWeaponType->pName ) );
				pToolTip->SetVal( L"engskillreq", pE->nSkillReq );
				if ( pE->nRequiredPerkID > 0 )
					pToolTip->SetVal( L"isengperkreq", GetDBString( 17133 ) );
			}
		}
	}
	else if ( pMelee )
	{
		NDb::CRPGMeleeWeapon *pM = pMelee->GetDBMeleeWeapon();
		pToolTip->SetText( GetDBString( pM->bThrowing ? 17040 : 4512 ) );
		pToolTip->SetVal( L"typename", GetDBString( pM->pWeaponType->pName ) );
		pToolTip->SetVal( L"damagemin", pM->nDmgMin );
		pToolTip->SetVal( L"damagemax", pM->nDmgMax );
		pToolTip->SetVal( L"attbonus", pM->nToHitBonus );
		pToolTip->SetVal( L"critbonus", pM->nCriticalBonus );
		if ( IsValid( pM->pItem ) )
		{
			int nW = pM->pItem->nWeight;
			pToolTip->SetVal( L"weight", GetDBString( nW < 400 ? 17134 : ( nW < 1000 ? 17135 : 17136 ) ) );
		}
	}
	else if ( pClip )
	{
		NDb::CRPGAmmo *pAmmo = pClip->GetDBAmmo();
		if ( pClip->GetDBClip()->nQuantity == 1 )
		{
			if ( IsValid( pAmmo->pExplosiveBullet ) )
			{
				pToolTip->SetText( GetDBString( pAmmo->pExplosiveBullet->nFragmentNumber < 20 ? 17430 : 17431 ) );
				MakeGrenadeToolTip( pToolTip, pAmmo->pExplosiveBullet );
			}
			else
			{
				pToolTip->SetText( GetDBString( 16978 ) );
				pToolTip->SetVal( L"ammotype", GetDBString( pAmmo->pName ) );
				pToolTip->SetVal( L"damagemin", pAmmo->nDmgMin );
				pToolTip->SetVal( L"damagemax", pAmmo->nDmgMax );
			}
		}
		else
		{
			pToolTip->SetText( GetDBString( 4470 ) );
			pToolTip->SetVal( L"ammotype", GetDBString( pAmmo->pName ) );
			pToolTip->SetVal( L"damagemin", pAmmo->nDmgMin );
			pToolTip->SetVal( L"damagemax", pAmmo->nDmgMax );
		}
	}
	else if ( pFirstAid )
	{
		NDb::CRPGFirstAid *pFA = pFirstAid->GetDBFirstAid();
		int nTextID = 0;
		switch ( pFA->effect )
		{
		case NDb::FAE_NORMAL:                 nTextID = 17042; break;
		case NDb::FAE_CRITICAL_ONLY:
		case NDb::FAE_REPAIR_PK:              nTextID = 17043; break;
		case NDb::FAE_TEMP_REMOVE_PENALTIES:  nTextID = 17127; break;
		case NDb::FAE_BOOST_VP:               nTextID = 17126; break;
		case NDb::FAE_TEMP_STOP_BLEEDING:     nTextID = 17128; break;
		case NDb::FAE_REMOVE_BLEEDING:        nTextID = 17129; break;
		}
		if ( nTextID )
			pToolTip->SetText( GetDBString( nTextID ) );
		pToolTip->SetVal( L"power", Float2Int( pFA->fPower ) );
		pToolTip->SetVal( L"duration", pFA->nDuration );
		pToolTip->SetVal( L"medskillreq", pFA->nRequiedSkill );
		pToolTip->SetVal( L"medskillbonus", pFA->nSkillModifier );
		pToolTip->SetVal( L"maxhealvp", pFA->nTotalHealVP );
		if ( pFA->nRequiredPerkID > 0 )
			pToolTip->SetVal( L"ismedperkreq", GetDBString( pFA->effect == NDb::FAE_REPAIR_PK ? 17133 : 17132 ) );
	}
	else if ( pTool )
	{
		NDb::CRPGTool *pT = pTool->GetDBItemInfo();
		pToolTip->SetText( GetDBString( 17038 ) );
		pToolTip->SetVal( L"engskillreq", pT->nNeededEngSkill );
		pToolTip->SetVal( L"engskillbonus", pT->nSkillModifForMineCleaning );
		if ( IsValid( pT->pNeededPerk ) )
			pToolTip->SetVal( L"isengperkreq", GetDBString( 17133 ) );
	}
	else if ( pPicklock )
	{
		NDb::CRPGPicklock *pP = pPicklock->GetDBPicklock();
		pToolTip->SetText( GetDBString( 17038 ) );
		pToolTip->SetVal( L"engskillreq", pP->nNeededEngSkill );
		pToolTip->SetVal( L"engskillbonus", pP->nAddToEngSkill );
		if ( IsValid( pP->pNeededPerk ) )
			pToolTip->SetVal( L"isengperkreq", GetDBString( 17133 ) );
	}
	else if ( pMine )
	{
		pToolTip->SetText( GetDBString( 17414 ) );
		if ( IsValid( pMine->GetDBItemInfo()->pExplosion ) )
			MakeGrenadeToolTip( pToolTip, pMine->GetDBItemInfo()->pExplosion );
	}
	else if ( pKey )
	{
		pToolTip->SetText( GetDBString( 19162 ) );
	}
	else if ( pClue )
	{
		pToolTip->SetText( GetDBString( 4358 ) );
	}
	else
	{
		pToolTip->SetText( IsValid( pRPGItem->pToolTip ) ? GetDBString( pRPGItem->pToolTip ) : GetDBString( 4690 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CShowItemModel::Draw @0x1c07b0: per-FRAME refresh of only the LIVE numbers -- everything else
// was written once by Set. weapon conv/range/damage + familiarity, melee familiarity, clip ammo,
// first-aid quantity, tool/picklock engquantity.
void CShowItemModel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pItem ) )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( pItem );
	CDynamicCast<NRPG::IMeleeWeaponItem> pMelee( pItem );
	CDynamicCast<NRPG::IClipItem> pClip( pItem );
	CDynamicCast<NRPG::IFirstAidItem> pFirstAid( pItem );
	CDynamicCast<NRPG::IToolItem> pTool( pItem );
	CDynamicCast<NRPG::IPicklockItem> pPicklock( pItem );

	if ( pWeapon && IsValid( pWeapon->GetDBWeapon()->pWeaponType ) )
	{
		NRPG::SWeaponInfo sInfo;
		pWeapon->GetInfo( &sInfo );
		pToolTip->SetVal( L"convamateur", sInfo.nMinRange );
		pToolTip->SetVal( L"convprofessional", sInfo.nMaxRange );
		pToolTip->SetVal( L"maxrange", sInfo.nMaxRange );
		pToolTip->SetVal( L"damagemin", sInfo.nDmgMin );
		pToolTip->SetVal( L"damagemax", sInfo.nDmgMax );
		pToolTip->SetVal( L"familiarity", IsValid( pUnit ) ? Float2Int( pUnit->GetRPG()->GetRPGUnit()->GetWeaponAdaptation( pItem ) ) : 0 );
	}
	else if ( pMelee )
	{
		pToolTip->SetVal( L"familiarity", IsValid( pUnit ) ? Float2Int( pUnit->GetRPG()->GetRPGUnit()->GetWeaponAdaptation( pItem ) ) : 0 );
	}
	else if ( pClip )
	{
		pToolTip->SetVal( L"ammomax", pClip->GetMaxIncQuantity() );
		pToolTip->SetVal( L"currammo", pClip->GetIncQuantity() );
	}
	else if ( pFirstAid )
	{
		// after the first-aid container refactor CFirstAidItem IS a CItemContainer<CSimpleCharge>,
		// so its live charge count comes through the IItemContainerInfo base (GetIncQuantity).
		CDynamicCast<NRPG::IItemContainerInfo> pCont( pItem );
		if ( pCont )
			pToolTip->SetVal( L"quantity", pCont->GetIncQuantity() );
	}
	else if ( pTool )
	{
		// Restored now that CToolItem IS a CItemContainer<CSimpleCharge> (retail shape). @0x1c07b0
		// RTTI-chains IFirstAidItem -> IToolItem -> IPicklockItem and refreshes all three through the
		// SAME vbtable+0x14 vbase call (IItemContainerInfo slot 0 = GetIncQuantity); tool and picklock
		// share the "engquantity" key. Previously elided because the Jan03 non-container CToolItem had
		// no per-tool charge count to report.
		pToolTip->SetVal( L"engquantity", pTool->GetIncQuantity() );
	}
	else if ( pPicklock )
	{
		pToolTip->SetVal( L"engquantity", pPicklock->GetIncQuantity() );   // IPicklockItem IS IItemContainerInfo
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
	CActionDecorator<CWindow>( sInfo, _pMission ), pMission( _pMission ), nWidth( _nWidth ), nHeight( _nHeight ), eCameraType( _eCameraType ), bAlwaysHilight( _bAlwaysHilight ), bTrackMouse( false )
{
	// retail @0x1c3390: the decorator base carries the mission link too (stored twice, +0x84/+0x88),
	// the hilight grid starts EMPTY (0x0; the default ctor's grid is 1x1), and the slot builds its
	// shared 3D preview view with the standard inventory ambient -- the same GetTAmbientLight(7)
	// idiom the sibling CUnitView/CInteractiveUnitView ctors use.
	hilights.SetSizes( 0, 0 );
	p3DView = NGScene::CreateNewFastInterfaceView();
	SRand rnd;
	p3DView->SetAmbient( NDb::GetTAmbientLight( 7 )->GetLight( &rnd ), NGScene::IGameView::LT_INVENTORY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1be990: a slot participates in the action-decorator state routing only for the item-drag
// state (CStateDragItem).
bool CSlot::CanHandleState( NGame::IState *pState ) const
{
	CDynamicCast<NGame::CStateDragItem> pDrag( pState );
	if ( pDrag )
		return true;

	return false;
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
	// retail @0x1c3150 stores the dims, then SKIPS the whole rebuild when the grid is already this
	// size (Jan03 always rebuilt). On save-load this is what preserves the DESERIALIZED hilight
	// cells: their pImage refs point at the CImage children restored inside pHilight's subtree, and
	// an unconditional rebuild would orphan them for fresh, unstyled images.
	const bool bUnchanged = ( _nWidth == hilights.GetXSize() ) && ( _nHeight == hilights.GetYSize() );
	nWidth = _nWidth;
	nHeight = _nHeight;
	if ( bUnchanged )
		return;

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
	// retail @0x1c53c0. Divergences from Jan03 (FOLLOW THE DECOMP): the MOUSEMOVE case only stores
	// sMousePoint (the release dropped Jan03's SetCursorInfo(SCursorInfo()) -- the cursor is driven
	// by the decorator/state routing below); TEMPLATELOADCOMPLETE additionally sets style bit 0x80
	// on the view child before sizing the grid; and the base call is the CActionDecorator base,
	// which is what routes drag events into the mission's CStateDragItem.
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			sMousePoint = SPoint( sEvent.nX, sEvent.nY );
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
			pSlotView->SetStyle( 0x80, true );   // retail literal: a style bit added past the Jan03 set
			SetSlotSize( nWidth, nHeight );
			break;
		}
	}

	return CActionDecorator<CWindow>::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::IsCompatibleItems @0x1c0e00: the "weapon x clip" pairing test behind the drag-compat
// YELLOW tint -- the weapon side's LOADED clip is ammo-group-compatible (color ignored) with the
// clip side. CSlot::Draw probes both argument orders, so the direction here is fixed.
static bool IsCompatibleItems( NRPG::IInventoryItem *pWeaponSide, NRPG::IInventoryItem *pClipSide )
{
	CDynamicCast<NRPG::IWeaponItem> pWeapon( pWeaponSide );
	CDynamicCast<NRPG::CClipItem> pClip( pClipSide );
	if ( !IsValid( pWeapon ) || !IsValid( pClip ) )
		return false;
	CDynamicCast<NRPG::CClipItem> pInner( pWeapon->GetInnerClip() );
	if ( !IsValid( pInner ) )
		return false;
	return pInner->IsCompatible( pClip.GetPtr(), false );
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
			// retail @0x1c34d0: the icon renders in the SLOT'S shared p3DView (tag 13) via
			// SetScene/bParentScene; GetUnit() is the slot vtbl+0x44 virtual.
			pItemModel->Set( p3DView, GetUnit(), sItem.pItem, eCameraType );
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

	// retail CSlot::Draw @0x1c34d0 (disasm; Jan03 painted plain blue unconditionally): the per-item
	// tint pass runs ONLY with exactly ONE selected unit. Base = transparent BLUE on occupied cells
	// (visible only when bAlwaysHilight); YELLOW (forced visible) when the cell's item pairs
	// ammo-compatibly with the dragged item; transparent RED (forced visible) when the unit CANNOT
	// USE the item -- a downed unit reddens everything, else the skill/perk gate of a first-aid kit
	// (repair kits gate on ENGINEERING, FAE_REPAIR_PK), tool, picklock, or engineer grenade fails.
	// Weapons/clips/keys/regular grenades are never reddened. Recomputed every frame, as in retail.
	vector< CPtr<NGame::IUnitTracker> > selUnits;
	pMission->GetSelectedUnits( &selUnits );
	if ( selUnits.size() == 1 )
	{
		NWorld::CUnit *pUnit = selUnits[0]->GetUnit();
		const bool bCanFight = IsValid( pUnit ) && pUnit->CanFight();
		NRPG::IUnitMissionInfo *pRPG = IsValid( pUnit ) ? pUnit->GetRPG() : 0;

		NWorld::SItem sDragInfo;
		NRPG::IInventoryItem *pDragItem = GetDragItem( &sDragInfo ) ? sDragInfo.pItem.GetPtr() : 0;

		for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
		{
			const SItem &sItem = itemsSet[nTemp];
			const SPoint &sPos = sItem.sPos;
			SPoint sSize = sItem.pItem->GetSize();
			sSize = SPoint( min( nWidth, sPos.x + sSize.x ) - sPos.x, min( nHeight, sPos.y + sSize.y ) - sPos.y );

			NGfx::SPixel8888 color( 0x1F, 0x1F, 0xFF, 0x2F );   // base: the Jan03 occupied-cell blue
			bool bVisible = bAlwaysHilight;

			// (a) retail addition: ammo-compat YELLOW while dragging (either pairing direction)
			if ( pDragItem && ( IsCompatibleItems( sItem.pItem, pDragItem ) || IsCompatibleItems( pDragItem, sItem.pItem ) ) )
			{
				color = NGfx::SPixel8888( 0xFF, 0xFF, 0x00, 0x2F );
				bVisible = true;
			}

			// (b) the can't-use predicate -> transparent RED (retail VA 0x5c3c5d..0x5c3e9e)
			bool bCantUse = !bCanFight;
			if ( !bCantUse && pRPG )
			{
				CDynamicCast<NRPG::IFirstAidItem> pFA( sItem.pItem );
				CDynamicCast<NRPG::IToolItem> pTool( sItem.pItem );
				CDynamicCast<NRPG::IPicklockItem> pPick( sItem.pItem );
				CDynamicCast<NRPG::IGrenadeItemInfo> pGren( sItem.pItem );
				if ( pFA )
				{
					NDb::CRPGFirstAid *pRec = pFA->GetDBFirstAid();
					// PK repair kits (FAE_REPAIR_PK) gate on ENGINEERING, everything else on MEDICINE
					NDb::ESkillType eSkill = ( pRec->effect == NDb::FAE_REPAIR_PK ) ? NDb::ST_ENGINEERING : NDb::ST_MEDICINE;
					if ( pRPG->GetSkillValue( eSkill ) < pRec->nRequiedSkill )
						bCantUse = true;
					else if ( pRec->nRequiredPerkID > 0 && !pRPG->GetRPGUnit()->HasPerk( pRec->nRequiredPerkID ) )
						bCantUse = true;
				}
				else if ( pTool )
				{
					NDb::CRPGTool *pRec = pTool->GetDBItemInfo();
					if ( pRPG->GetSkillValue( NDb::ST_ENGINEERING ) < pRec->nNeededEngSkill )
						bCantUse = true;
					else if ( IsValid( pRec->pNeededPerk ) && !pRPG->GetRPGUnit()->HasPerk( pRec->pNeededPerk->GetRecordID() ) )
						bCantUse = true;
				}
				else if ( pPick )
				{
					NDb::CRPGPicklock *pRec = pPick->GetDBPicklock();
					if ( pRPG->GetSkillValue( NDb::ST_ENGINEERING ) < pRec->nNeededEngSkill )
						bCantUse = true;
					else if ( IsValid( pRec->pNeededPerk ) && !pRPG->GetRPGUnit()->HasPerk( pRec->pNeededPerk->GetRecordID() ) )
						bCantUse = true;
				}
				else if ( pGren )
				{
					NDb::CRPGEngGrenade *pRec = pGren->GetDBEngGrenade();
					if ( IsValid( pRec ) )   // NULL for regular grenades -> never red
					{
						if ( pRPG->GetSkillValue( NDb::ST_ENGINEERING ) < pRec->nSkillReq )
							bCantUse = true;
						else if ( pRec->nRequiredPerkID > 0 && !pRPG->GetRPGUnit()->HasPerk( pRec->nRequiredPerkID ) )
							bCantUse = true;
					}
				}
			}
			if ( bCantUse )
			{
				color = NGfx::SPixel8888( 0xFF, 0x00, 0x00, 0x2F );
				bVisible = true;
			}

			for ( int nTempY = 0; nTempY < sSize.y; nTempY++ )
			{
				for ( int nTempX = 0; nTempX < sSize.x; nTempX++ )
				{
					SHilight &sHilight = hilights[sPos.y + nTempY][sPos.x + nTempX];
					sHilight.nID = nTemp;
					sHilight.pImage->SetColor( color );
					sHilight.pImage->SetStyle( STYLE_VISIBLE, bVisible );
				}
			}
		}
	}

	// NOTE: the SItem built below deliberately carries eType = HAND -- it describes a prospective MOVE
	// whose SOURCE is the hand, which is not the same thing as sItemInfo.eType (the in-hand item's
	// drag ORIGIN, e.g. the slot it was pulled out of).
	NWorld::SItem sItemInfo;
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

	// retail @0x1c34d0 tail (VA 0x5c417f..0x5c441f): the 2D pass (children incl. every parent-scene
	// item icon's transform fold) draws FIRST, then the slot draws its shared p3DView ONCE for all
	// item meshes between the two depth-clear quads.
	CTransformStack ts;
	MakeProjection( &ts );

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	CWindow::Draw( sTime, pView );
	CreateClearRect( pView, 1.0f );
	pView->Flush();

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &ts;
	drawInfo.vOrigin = CVec2( sWindow.x1 / 1024.0f, sWindow.y1 / 768.0f );
	drawInfo.vSize = CVec2( sWindow.Width() / 1024.0f, sWindow.Height() / 768.0f );
	drawInfo.bOverlay = true;
	p3DView->Draw( drawInfo );

	CreateClearRect( pView, 0.0f );
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
// retail NUI::CSlot::GetDragItem @0x1beb80: GetActivePlayer()->GetPlayer()->GetInHandItem(pInfo),
// dispatched through IPlayer vtbl+0x30 (slot 12). Retail additionally gates on IMission vtbl+0x40
// before touching the active player; that gate is absent here as it was before this change (a
// pre-existing divergence, not part of the SItemInfo->SItem migration).
bool CSlot::GetDragItem( NWorld::SItem *pInfo )
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
	CPtr<CText> pText;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Create one text state on a button and size both the state and the button to fit the caption.
// Mirrors NUI::AddDynamicState @0x1c5500: a CText (markup text, so it measures its own natural
// size via GetRealSize) is placed in the new state window; the state is sized to text+8 x lineHeight,
// and the button grows to fit its widest caption. (The binary's exact button-width formula is
// Ghidra-ambiguous; fit-to-widest-caption is the observable behaviour -- buttons sized to their text.)
static void AddDynamicState( CButton *pButton, int nState, const wstring &wsText, int nLineHeight, vector<SButtonsLineReflow> *pReflow )
{
	SButtonsLineReflow sEntry;
	sEntry.pState = pButton->AddState( nState );
	CText *pText = new CText( SWindowInfo( sEntry.pState, SPoint( 0, 0 ), SPoint( 1024, nLineHeight ), "text" ) );
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
REGISTER_SAVELOAD_CLASS( 0xB3915110, CSlotInfo );   // retail id ($E-registered, gen/classreg.json)
REGISTER_SAVELOAD_CLASS( 0xb3601001, CButtonsLine );
