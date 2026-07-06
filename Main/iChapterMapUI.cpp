#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "MemObject.h"
#include "RPGGlobal.h"
#include "Sound.h"
#include "PolyUtils.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iShowClue.h"
#include "ChapterInfo.h"
#include "iChapterMapUI.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataScenario.h"
#include "scFlowChartItems.h"
#include "scScenarioTracker.h"
#include "UIWrap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int
	N_ZONE_SIZE									= 69,
	N_RANDOMZONE_SIZE						= 49,
	N_RANDOMZONE_TTL						= 10000,	// TimeToLife
	N_RANDOMZONE_MIN_DIST				= 8,
	N_RANDOMZONE_MAX_DIST				= 256,
	N_RANDOMZONE_ROLLTRY_COUNT	= 100;
const float
	F_PATH_CHECKLEN = 50;
const CVec4
	V4_SECTOR_NORMALCOLOR = CVec4( 0.3f, 1, 0.3f, 0.5f ),
	V4_SECTOR_SELECTEDCOLOR = CVec4( 1, 0.3f, 0.3f, 0.5f );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCluesSort
{
	bool operator()( NScenario::CScenarioClue *p1, NScenario::CScenarioClue *p2 ) const 
	{ 
		return p1->GetOpenOrder() < p2->GetOpenOrder(); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTeamMarker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTeamMarker: public CButton
{
	OBJECT_BASIC_METHODS(CTeamMarker);
public:
	enum EMode
	{
		MODE_MOVE,
		MODE_NORMAL,
		MODE_ZONE
	};

private:
	ZDATA_(CButton)
	EMode eMode, eTargetMode;
	STime sMorphTime, sFlashTime;
	////
	CObj<CImageDraw> pMoving;
	CObj<CImageDraw> pZone;
	CObj<CImageDraw> pZoneFlash;
	CObj<CImageDraw> pNormal;
	CObj<CImageDraw> pNormalFlash;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&eMode); f.Add(3,&eTargetMode); f.Add(4,&sMorphTime); f.Add(5,&sFlashTime); f.Add(6,&pMoving); f.Add(7,&pZone); f.Add(8,&pZoneFlash); f.Add(9,&pNormal); f.Add(10,&pNormalFlash); f.Add(11,&pToolTip); return 0; }
	CPtr<CToolTip> pToolTip;

public:
	CTeamMarker() {}
	CTeamMarker( const SWindowInfo &sInfo );

	void SetMode( EMode eMode );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTeamMarker::CTeamMarker( const SWindowInfo &sInfo ):
	CButton( sInfo ), eMode( MODE_NORMAL ), eTargetMode( MODE_NORMAL ), sMorphTime( 0 ), sFlashTime( 0 )
{
	CPtr<NDb::CUITexture> pNormalTexture = NDb::GetUITexture( 524 );
	if ( IsValid( pNormalTexture ) )
		SetSize( SPoint( pNormalTexture->nWidth, pNormalTexture->nHeight ) );

	pMoving = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 525 ) );
	pZone = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 530 ) );
	pZoneFlash = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 531 ) );
	pNormal = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 524 ) );
	pNormalFlash = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 526 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMarker::SetMode( EMode _eMode )
{
	eTargetMode = _eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMarker::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	float fNormal = 0.0f, fMoving = 0.0f, fFlash = 0.0f;

	if ( ( eMode != eTargetMode ) && ( sMorphTime + N_STANDART_MORPHTIME > sTime ) )
	{
		sFlashTime = sTime;

		switch( eTargetMode )
		{
		case MODE_MOVE:
			fMoving = float( sTime - sMorphTime ) / N_STANDART_MORPHTIME;
			fNormal = 1.0f;
			break;
		case MODE_ZONE:
		case MODE_NORMAL:
			fNormal = float( sTime - sMorphTime ) / N_STANDART_MORPHTIME;
			fMoving = 1.0f;
			break;
		}
	}
	else
	{
		eMode = eTargetMode;
		sMorphTime = sTime;

		switch( eMode )
		{
		case MODE_MOVE:
			fMoving = 1.0f;
			break;
		case MODE_ZONE:
		case MODE_NORMAL:
			fNormal = 1.0f;
			fFlash = float( ( sTime - sFlashTime ) % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
			if ( fFlash > 1 )
				fFlash = 2 - fFlash;
			if ( IsMouseCover() )
			{
				fFlash = 1.0f;
				sFlashTime = sTime - N_STANDART_FLASHTIME;
			}
			break;
		}
	}

	pMoving->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fMoving ) );
	pZone->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fNormal ) );
	pZoneFlash->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fFlash ) );
	pNormal->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fNormal ) );
	pNormalFlash->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fFlash ) );

	switch( eTargetMode )
	{
	case MODE_MOVE:
		if ( eMode == MODE_ZONE )
			pZone->Draw( this, sTime, pView );
		else if ( eMode == MODE_NORMAL )
			pNormal->Draw( this, sTime, pView );

		pMoving->Draw( this, sTime, pView );

		if ( eMode == MODE_ZONE )
			pZoneFlash->Draw( this, sTime, pView );
		else if ( eMode == MODE_NORMAL )
			pNormalFlash->Draw( this, sTime, pView );
		break;
	case MODE_ZONE:
		pMoving->Draw( this, sTime, pView );
		pZone->Draw( this, sTime, pView );
		pZoneFlash->Draw( this, sTime, pView );
		break;
	case MODE_NORMAL:
		pMoving->Draw( this, sTime, pView );
		pNormal->Draw( this, sTime, pView );
		pNormalFlash->Draw( this, sTime, pView );
		break;
	}

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDescriptionText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDescriptionText: public CWindow
{
	OBJECT_BASIC_METHODS(CDescriptionText);
public:
	enum EMode
	{
		MODE_HIDDEN,
		MODE_VISIBLE
	};

private:
	ZDATA_(CWindow)
	EMode eMode, eTargetMode;
	STime sMorphTime;
	float fCoeff;
	////
	CPtr<CText> pText;
	CPtr<CImage> pBackground;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&eMode); f.Add(3,&eTargetMode); f.Add(4,&sMorphTime); f.Add(5,&fCoeff); f.Add(6,&pText); f.Add(7,&pBackground); return 0; }

public:
	CDescriptionText() {}
	CDescriptionText( const SWindowInfo &sInfo );

	void Set( EMode eMode, const wstring &wsText = L"" );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDescriptionText::CDescriptionText( const SWindowInfo &sInfo ):
	CWindow( sInfo ), eMode( MODE_HIDDEN ), eTargetMode( MODE_HIDDEN ), sMorphTime( 0 ), fCoeff( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDescriptionText::Set( EMode _eMode, const wstring &wsText )
{
	if ( ( eTargetMode != _eMode ) && ( eTargetMode != eMode ) )
		eMode = eTargetMode;

	eTargetMode = _eMode;
	pText->SetText( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDescriptionText::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pText = GetUIWindow<CText>( this, "text" );
			pBackground = GetUIWindow<CImage>( this, "background" );
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDescriptionText::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( eTargetMode != eMode )
	{
		if ( eTargetMode == MODE_HIDDEN )
		{
			fCoeff = CalcFlashCoeff( fCoeff, 0, sTime, sMorphTime );
			if ( fCoeff == 0 )
				eMode = eTargetMode;
		}
		else
		{
			fCoeff = CalcFlashCoeff( fCoeff, 1.0f, sTime, sMorphTime );
			if ( fCoeff == 1.0f )
				eMode = eTargetMode;
		}

		pText->SetStyle( STYLE_VISIBLE, false );
		pBackground->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
		SetStyle( STYLE_VISIBLE, true );
	}
	else
	{
		eMode = eTargetMode;

		pText->SetStyle( STYLE_VISIBLE, true );
		pBackground->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );

		if ( eMode == MODE_HIDDEN )
			SetStyle( STYLE_VISIBLE, false );
		else
			SetStyle( STYLE_VISIBLE, true );
	}

	sMorphTime = sTime;
	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterSector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChapterSector: public CWindow
{
private:
	ZDATA_(CWindow)
	SChapterSector sSector;
	////
	bool bVisible;
	bool bSelected;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&sSector); f.Add(3,&bVisible); f.Add(4,&bSelected); return 0; }

public:
	CChapterSector() {}
	CChapterSector( const SWindowInfo &sInfo, const SChapterSector &sSector );

	virtual bool IsRecommended() const;
	virtual bool GetDescription( wstring *psText ) const;

	bool IsVisible() const;
	void SetVisible( bool bState );

	bool IsSelected() const;
	void SetSelected( bool bState );

	const SChapterSector& GetSector() const;

	virtual void UpdateSector( const STime &sTime, const CVec2 &vTeamPose )= 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CChapterSector::CChapterSector( const SWindowInfo &sInfo, const SChapterSector &_sSector ):
	CWindow( sInfo ), sSector( _sSector ), bVisible( false ), bSelected( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterSector::IsRecommended() const
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterSector::GetDescription( wstring *psText ) const
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterSector::IsVisible() const
{
	return bVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSector::SetVisible( bool bState )
{
	bVisible = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterSector::IsSelected() const
{
	return bSelected;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSector::SetSelected( bool bState )
{
	bSelected = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SChapterSector& CChapterSector::GetSector() const
{
	return sSector;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CZoneSector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CZoneSector: public CChapterSector
{
	OBJECT_BASIC_METHODS(CZoneSector)
private:
	ZDATA_(CChapterSector)
	CPtr<NGame::IChapterMap> pChapter;
	CPtr<NScenario::CScenarioZone> pZone;
	////
	bool bVisited;
	bool bRecommended;
	////
	float fCoeff;
	STime sMorphTime;
	CObj<CImageDraw> pFlash;
	CObj<CImageDraw> pNormal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CChapterSector*)this); f.Add(2,&pChapter); f.Add(3,&pZone); f.Add(4,&bVisited); f.Add(5,&bRecommended); f.Add(6,&fCoeff); f.Add(7,&sMorphTime); f.Add(8,&pFlash); f.Add(9,&pNormal); return 0; }

public:
	CZoneSector() {}
	CZoneSector( const SWindowInfo &sInfo, NGame::IChapterMap *pChapter, const SChapterSector &sSector );

	bool IsRecommended() const;
	bool GetDescription( wstring *psText ) const;

	void UpdateSector( const STime &sTime, const CVec2 &vTeamPos );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CZoneSector::CZoneSector( const SWindowInfo &sInfo, NGame::IChapterMap *_pChapter, const SChapterSector &sSector ):
	CChapterSector( sInfo, sSector ), pChapter( _pChapter ), bVisited( false ), bRecommended( false ), fCoeff( 0 ), sMorphTime( 0 )
{
	pFlash = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ) );
	pNormal = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ) );

	pZone = pChapter->GetGlobalGame()->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( GetSector().nTemplate ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZoneSector::IsRecommended() const
{
	return bRecommended;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZoneSector::GetDescription( wstring *psText ) const
{
	if ( GetSector().nDescriptionID == -1 )
		return false;

	*psText = GetDBString( GetSector().nDescriptionID );

	CPtr<NRPG::CGlobalGame> pGame = pChapter->GetGlobalGame();
	if ( IsValid( pZone ) )
	{
		list< CPtr<NScenario::CScenarioClue> > cluesList;
		pGame->pScenarioTracker->GetAvailableClues( &cluesList );
		cluesList.sort( SCluesSort() );

		int nCount = 0;
		wstring wsDescr;
		for ( list< CPtr<NScenario::CScenarioClue> >::const_iterator iTemp = cluesList.begin(); iTemp != cluesList.end(); iTemp++ )
		{
			if ( pGame->pScenarioTracker->GetZoneInWhichClueWasFound( (*iTemp) ) == pZone )
			{
				wsDescr += GetDBString( (*iTemp)->GetDBClue()->pDescription ).c_str();
				wsDescr += L"<br>";
				nCount++;
			}
		}

		if ( nCount > 0 )
		{
			*psText += NStr::Format( L"<br><br>%d Clues found:<br>", nCount );
			*psText += wsDescr;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CZoneSector::UpdateSector( const STime &sTime, const CVec2 &vTeamPos )
{
	if ( !IsValid( pZone ) )
		return;

	CPtr<NRPG::CGlobalGame> pGame = pChapter->GetGlobalGame();

	bool bHit = HitTest( vTeamPos.x, vTeamPos.y );
	if ( bHit )
		pGame->pScenarioTracker->RevealZone( pZone );

	list<CPtr<NScenario::CScenarioZone> > zonesList;
	pGame->pScenarioTracker->GetAvailableZones( &zonesList );

	bRecommended = false;
	SetVisible( false );
	if ( find( zonesList.begin(), zonesList.end(), pZone ) != zonesList.end() )
	{
		SetVisible( true );
		if ( pGame->pScenarioTracker->GetRecommendedZone( pGame->players.front() ) == pZone )
			bRecommended = true;
	}

	bVisited = pZone->IsPassed();

	if ( bHit && IsVisible() && NGlobal::GetVar( "cheat_zoneautocomplete" ).GetFloat() != 0 )
		pGame->pScenarioTracker->CheatOpenZone( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CZoneSector::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsVisible() )
		return;

	CPtr<NDb::CUITexture> pNormalTexture, pFlashTexture;
	if ( !bVisited )
	{
		pFlashTexture = NDb::GetUITexture( 546 );
		pNormalTexture = NDb::GetUITexture( 520 );
	}
	else
	{
		pFlashTexture = NDb::GetUITexture( 547 );
		pNormalTexture = NDb::GetUITexture( 521 );
	}

	float fTargetCoeff = IsSelected() ? 1.0f : 0.0f;
	if ( !IsSelected() && bRecommended )
	{
		fTargetCoeff = float( sTime % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
		if ( fTargetCoeff > 1 )
			fTargetCoeff = 2 - fTargetCoeff;
	}
	fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
	sMorphTime = sTime;

	pNormal->SetImage( pNormalTexture );
	pNormal->Draw( this, sTime, pView );

	pFlash->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
	pFlash->SetImage( pFlashTexture );
	pFlash->Draw( this, sTime, pView );

	CChapterSector::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRandomSector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRandomSector: public CChapterSector
{
	OBJECT_BASIC_METHODS(CRandomSector)
private:
	ZDATA_(CChapterSector)
	CPtr<NGame::IChapterMap> pChapter;
	////
	STime sUpdateTime;
	CTRect<float> sZone;
	////
	float fCoeff;
	STime sMorphTime;
	CObj<CImageDraw> pMarker;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CChapterSector*)this); f.Add(2,&pChapter); f.Add(3,&sUpdateTime); f.Add(4,&sZone); f.Add(5,&fCoeff); f.Add(6,&sMorphTime); f.Add(7,&pMarker); return 0; }

public:
	CRandomSector() {}
	CRandomSector( const SWindowInfo &sInfo, NGame::IChapterMap *pChapter, const SChapterSector &sSector );

	void UpdateSector( const STime &sTime, const CVec2 &vTeamPos );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRandomSector::CRandomSector( const SWindowInfo &sInfo, NGame::IChapterMap *_pChapter, const SChapterSector &sSector ):
	CChapterSector( sInfo, sSector ), pChapter( _pChapter ), sUpdateTime( 0 ), sZone( 0, 0, 0, 0 ), fCoeff( 0 ), sMorphTime( 0 )
{
	for ( int nPoint = 0; nPoint < sSector.pointsSet.size(); nPoint++ )
	{
		const CVec2 &vSectorPoint = sSector.pointsSet[nPoint];
		sZone.x1 = min( sZone.x1, vSectorPoint.x );
		sZone.x2 = max( sZone.x2, vSectorPoint.x );
		sZone.y1 = min( sZone.y1, vSectorPoint.y );
		sZone.y2 = max( sZone.y2, vSectorPoint.y );
	}

	pMarker = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 519 ) );

	CPtr<CToolTip> pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	pToolTip->SetText( GetDBString( 5343 ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRandomSector::UpdateSector( const STime &sTime, const CVec2 &vTeamPos )
{
	static SRand sRand;

	vector<string> templParams;
	if ( HitTest( vTeamPos.x, vTeamPos.y ) )
		NMainLoop::Command( new NGame::CICBeginMission( GetSector().nTemplate, -1, templParams, pChapter->GetGlobalGame() ) );

	if ( sUpdateTime > sTime )
		return;

	sUpdateTime = sTime + N_RANDOMZONE_TTL * sRand.GetFloat( 0.5f, 1.0f );
	if ( !IsVisible() )
	{
		if ( sRand.Get( 100 ) < GetSector().nProbability )
			return;

		for ( int nTemp = 0; nTemp < N_RANDOMZONE_ROLLTRY_COUNT; nTemp++ )
		{
			CVec2 vPoint = CVec2( sRand.GetFloat( sZone.x1, sZone.x2 ), sRand.GetFloat( sZone.y1, sZone.y2 ) );

			if ( !IsPointInPolygon( GetSector().pointsSet, vPoint ) )
				continue;

			float fDist = fabs( vTeamPos - vPoint );
			if ( ( fDist < N_RANDOMZONE_MIN_DIST ) || ( fDist > N_RANDOMZONE_MAX_DIST ) )
				continue;

			SetVisible( true );

			SPoint sMarkerPos;
			const SPoint &sSize = GetSize();
			GetParent()->ScreenToClient( SPoint( vPoint.x - sSize.x / 2, vPoint.y - sSize.y / 2 ), &sMarkerPos );
			SetStyle( STYLE_VISIBLE, true );
			SetPosition( sMarkerPos );
		}
	}
	else
	{
		if ( sRand.Get( 100 ) < GetSector().nProbability / 4 )
			return;

		SetVisible( false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRandomSector::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsVisible() )
		fCoeff = CalcFlashCoeff( fCoeff, 1.0f, sTime, sMorphTime );
	else
		fCoeff = CalcFlashCoeff( fCoeff, 0.0f, sTime, sMorphTime );

	sMorphTime = sTime;

	CChapterSector::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRandomSector::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsVisible() && ( fCoeff == 0.0f ) )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	pMarker->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
	pMarker->Draw( this, sTime, pView );

	CChapterSector::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExitZoneSector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExitZoneSector: public CChapterSector
{
	OBJECT_BASIC_METHODS(CExitZoneSector)
private:
	ZDATA_(CChapterSector)
	CPtr<NGame::IChapterMap> pChapter;
	////
	bool bRecommended;
	////
	float fCoeff;
	STime sMorphTime;
	CObj<CImageDraw> pFlash;
	CObj<CImageDraw> pNormal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CChapterSector*)this); f.Add(2,&pChapter); f.Add(3,&bRecommended); f.Add(4,&fCoeff); f.Add(5,&sMorphTime); f.Add(6,&pFlash); f.Add(7,&pNormal); return 0; }

public:
	CExitZoneSector() {}
	CExitZoneSector( const SWindowInfo &sInfo, NGame::IChapterMap *pChapter, const SChapterSector &sSector );

	void UpdateSector( const STime &sTime, const CVec2 &vTeamPos );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CExitZoneSector::CExitZoneSector( const SWindowInfo &sInfo, NGame::IChapterMap *_pChapter, const SChapterSector &sSector ):
	CChapterSector( sInfo, sSector ), pChapter( _pChapter ), bRecommended( false ), fCoeff( 0 ), sMorphTime( 0 )
{
	pFlash = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 561 ) );
	pNormal = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 560 ) );

	CPtr<CToolTip> pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	pToolTip->SetText( GetDBString( 7542 ) );
	SetToolTip( pToolTip );

	SetVisible( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExitZoneSector::UpdateSector( const STime &sTime, const CVec2 &vTeamPos )
{
	CPtr<NScenario::CScenarioZone> pRecomendedZone = pChapter->GetGlobalGame()->pScenarioTracker->GetRecommendedZone( pChapter->GetGlobalPlayer() );

	list<CPtr<NScenario::CScenarioZone> > zonesList;
	pChapter->GetGlobalGame()->pScenarioTracker->GetAvailableZones( &zonesList );

	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo = pChapter->GetChapterInfo();
	pChapterInfo.Refresh();
	CObj<CChapterInfo> pInfo = pChapterInfo->GetValue();

	bRecommended = IsValid( pRecomendedZone );
	for ( int nTemp = 0; nTemp < pInfo->sectorsSet.size(); nTemp++ )
	{
		SChapterSector &sChapterSector = pInfo->sectorsSet[nTemp];
		if ( sChapterSector.eType != ZONE )
			continue;

		CPtr<NScenario::CScenarioZone> pZone = pChapter->GetGlobalGame()->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( sChapterSector.nTemplate ) );
		if ( ( find( zonesList.begin(), zonesList.end(), pZone ) != zonesList.end() ) && ( pRecomendedZone == pZone ) )
			bRecommended = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExitZoneSector::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	float fTargetCoeff = IsSelected() ? 1.0f : 0.0f;
	if ( !IsSelected() && bRecommended )
	{
		fTargetCoeff = float( sTime % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
		if ( fTargetCoeff > 1 )
			fTargetCoeff = 2 - fTargetCoeff;
	}
	fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
	sMorphTime = sTime;

	pNormal->Draw( this, sTime, pView );

	pFlash->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
	pFlash->Draw( this, sTime, pView );
	CChapterSector::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CChapterMapUI::CChapterMapUI( const SWindowInfo &sInfo, NGame::IChapterMap *_pChapter ):
	CWindow( sInfo ), pChapter( _pChapter )
{
	// retail CChapterMapUI ctor @0x1aa1b0: disasm @0x5aa2a3 `mov ecx,1` -> NDb::GetUICursor(1) =
	// UICursors row 1 "xz" (UITexture 295, NormalPen.cur) -- the chapter-map default cursor.
	// (492 was the pre-remap arbitrary id = HitLocationLeftArm.cur.)
	sCursor = SCursorInfo( NDb::GetUITexture( 295 ) );

	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo = pChapter->GetChapterInfo();
	pChapterInfo.Refresh();
	pInfo = pChapterInfo->GetValue();

	vCurrentPos = pChapter->GetGlobalGame()->vChapterPos;
	vTargetPos = vCurrentPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMapUI::SetTarget( const CVec2 &_vTargetPos )
{
	vTargetPos = _vTargetPos;
	float fXDelta = vTargetPos.x - vCurrentPos.x;
	float fYDelta = vTargetPos.y - vCurrentPos.y;
	if ( abs( fYDelta ) > abs( fXDelta ) )
	{
		fXK = fXDelta / abs( fYDelta );
		fYK = fYDelta / abs( fYDelta );
	}
	else
	{
		fXK = fXDelta / abs( fXDelta );
		fYK = fYDelta / abs( fXDelta );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterMapUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "action" )
			{
				bool bHandled = false;
				for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
				{
					if ( !sectorsSet[nTemp]->HitTest( vCurrentPos.x, vCurrentPos.y ) )
						continue;

					const SChapterSector &sSector = sectorsSet[nTemp]->GetSector();

					if ( sSector.eType == ZONE )
					{
						vector<string> templParams;
						CPtr<NScenario::CScenarioZone> pZone = pChapter->GetGlobalGame()->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( sSector.nTemplate ) );
						if ( IsValid( pZone ) )
						{
							bHandled = true;
							NMainLoop::Command( new NGame::CICBeginMission( pZone, -1, templParams, pChapter->GetGlobalGame() ) );
						}
					}
					else if ( sSector.eType == EXITZONE )
					{
						bHandled = true;
						NMainLoop::Command( new NGame::CICContinueGlobal( pChapter->GetGlobalGame() ) );
					}
				}

				if ( !bHandled )
				{
					SRand sRand;
					NDb::CChapterMap *pChapterMap = pChapter->GetChapterMap();
					if ( !pChapterMap->campZonesSet.empty() )
					{
						int nID = sRand.Get( pChapterMap->campZonesSet.size() );

						vector<string> templParams;
						NMainLoop::Command( new NGame::CICBeginMission( pChapterMap->campZonesSet[nID], -1, templParams, pChapter->GetGlobalGame() ) );
					}
				}

				return true;
			}

			break;
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( sCursor );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pShowGlobal = new CFlashButton( sEvent.pLoader->GetControl( "showglobal" ) );

			pTextLeft = new CDescriptionText( sEvent.pLoader->GetControl( "text_left" ) );
			pTextRight = new CDescriptionText( sEvent.pLoader->GetControl( "text_right" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pMapView = GetUIWindow<CWindow>( this, "view" );

			pBackground = GetUIWindow<CImage>( this, "background" );
			pBackground->SetImage( pChapter->GetChapterMap()->pBackground );

			pTeamMarker = new CTeamMarker( SWindowInfo( pMapView, SPoint( vCurrentPos.x, vCurrentPos.y ), SPoint( 0, 0 ), "action", STYLE_ENABLED | STYLE_VISIBLE ) );

			sectorsSet.reserve( pInfo->sectorsSet.size() );
			for ( int nTemp = 0; nTemp < pInfo->sectorsSet.size(); nTemp++ )
			{
				SChapterSector &sChapterSector = pInfo->sectorsSet[nTemp];
				if ( sChapterSector.pointsSet.empty() )
					continue;

				switch( sChapterSector.eType )
				{
				case ZONE:
					{
						SPoint sPosition;
						const CVec2 &vPos = sChapterSector.pointsSet.front();
						pMapView->ScreenToClient( SPoint( vPos.x, vPos.y ), &sPosition );
						sectorsSet.push_back( new CZoneSector( SWindowInfo( pMapView, sPosition, SPoint( N_ZONE_SIZE, N_ZONE_SIZE ), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ), pChapter, sChapterSector ) );
						break;
					}
				case RANDOM:
					{
						sectorsSet.push_back( new CRandomSector( SWindowInfo( pMapView, SPoint( 0, 0 ), SPoint( N_RANDOMZONE_SIZE, N_RANDOMZONE_SIZE ), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ), pChapter, sChapterSector ) );
						break;
					}
				case EXITZONE:
					{
						SPoint sPosition;
						const CVec2 &vPos = sChapterSector.pointsSet.front();
						pMapView->ScreenToClient( SPoint( vPos.x, vPos.y ), &sPosition );
						sectorsSet.push_back( new CExitZoneSector( SWindowInfo( pMapView, sPosition, SPoint( N_ZONE_SIZE, N_ZONE_SIZE ), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ), pChapter, sChapterSector ) );
						break;
					}
				default:
					ASSERT( 0 );
					break;
				}
			}

			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	{
		if ( pMapView->HitTest( sEvent.nX, sEvent.nY ) )
			SetTarget( CVec2( sEvent.nX, sEvent.nY ) );

		return true;
	}
	case EVENT_LBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONUP:
	case EVENT_RBUTTONDOWN:
	case EVENT_RBUTTONDBLCLK:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMapUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	bool bMoving = false;
	STime sTargetTime = sTime;
	if ( fabs2( vTargetPos - vCurrentPos ) > 1 )
	{
		bMoving = true;

		while ( ( fabs( vTargetPos - vCurrentPos ) > 1 ) && ( sLastUpdateTime < sTargetTime ) )
		{
			vCurrentPos.x += fXK;
			vCurrentPos.y += fYK;
			fPassedPathLen += ( fXK + fYK );

			SPoint sTile( vCurrentPos.x, vCurrentPos.y );
			float fSpeed = 20;

			sLastUpdateTime = sLastUpdateTime + 1000.0f / fSpeed;

			if ( fPassedPathLen > F_PATH_CHECKLEN )
			{
				fPassedPathLen -= F_PATH_CHECKLEN;

				for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
					sectorsSet[nTemp]->UpdateSector( sTime, vCurrentPos );
			}
		}
	}

	bool bHideLeft = true, bHideRight = true;
	const SPoint &sCursorPos = GetInterface()->GetCursorPos();
	for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
	{
		CChapterSector *pSector = sectorsSet[nTemp];
		pSector->SetSelected( false );
		if ( pSector->IsVisible() && pSector->HitTest( sCursorPos.x, sCursorPos.y ) )
		{
			wstring wsText;
			if ( pSector->GetDescription( &wsText ) )
			{
				if ( sCursorPos.x > 512 )
				{
					bHideLeft = false;
					pTextLeft->Set( CDescriptionText::MODE_VISIBLE, wsText );
				}
				else
				{
					bHideRight = false;
					pTextRight->Set( CDescriptionText::MODE_VISIBLE, wsText );
				}
			}

			pSector->SetSelected( true );
		}

		pSector->UpdateSector( sTime, vCurrentPos );
	}
	if ( bHideLeft )
		pTextLeft->Set( CDescriptionText::MODE_HIDDEN );
	if ( bHideRight )
		pTextRight->Set( CDescriptionText::MODE_HIDDEN );

	pChapter->GetGlobalGame()->vChapterPos = vCurrentPos;
	sLastUpdateTime = sTargetTime;

	SPoint sPoint;
	pMapView->ScreenToClient( SPoint( vCurrentPos.x, vCurrentPos.y ), &sPoint );

	bool bHitSector = false;
	for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
	{
		if ( !sectorsSet[nTemp]->HitTest( vCurrentPos.x, vCurrentPos.y ) )
			continue;

		bHitSector = true;
		break;
	}

	const SPoint &sSize = pTeamMarker->GetSize();
	pTeamMarker->SetPosition( SPoint( sPoint.x - sSize.x / 2, sPoint.y - sSize.y / 2 ) );
	pTeamMarker->SetMode( bMoving ? CTeamMarker::MODE_MOVE : bHitSector ? CTeamMarker::MODE_ZONE : CTeamMarker::MODE_NORMAL );

	list< CPtr<NScenario::CScenarioClue> > cluesList;
	pChapter->GetGlobalGame()->pScenarioTracker->GetAvailableClues( &cluesList );
	cluesList.sort( SCluesSort() );
	for( list< CPtr<NScenario::CScenarioClue> >::const_iterator iTemp = cluesList.begin(); iTemp != cluesList.end(); iTemp++ )
	{
		if ( !(*iTemp)->IsJustFound() )
			continue;

		NMainLoop::Command( new NGame::CICShowClue( pChapter->GetGlobalGame(), (*iTemp) ) );
		(*iTemp)->SetJustFound( false );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB2606150, CTeamMarker );
REGISTER_SAVELOAD_CLASS( 0xB2606152, CChapterMapUI );
REGISTER_SAVELOAD_CLASS( 0xB2606153, CZoneSector );
REGISTER_SAVELOAD_CLASS( 0xB2606154, CRandomSector );
REGISTER_SAVELOAD_CLASS( 0xB2606155, CExitZoneSector );
REGISTER_SAVELOAD_CLASS( 0xB2606156, CDescriptionText );
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(Gfx)
	REGISTER_VAR( "cheat_zoneautocomplete", NULL, 0, false )
FINISH_REGISTER
