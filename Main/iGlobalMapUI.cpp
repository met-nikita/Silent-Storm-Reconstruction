#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "MemObject.h"
#include "PolyUtils.h"
#include "RPGGlobal.h"
#include "ChapterInfo.h"
#include "Sound.h"
#include "Interface.h"
#include "iMission.h"
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iCommonUI.h"
#include "iGlobalMapUI.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataScenario.h"
#include "..\DBFormat\DataInterface.h"
#include "scFlowChartItems.h"
#include "scScenarioTracker.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, CChapterInfoLoader> shareChapterInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ISector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalSector: public CWindow
{
private:
	ZDATA_(CWindow)
	SGlobalSector sSector;
	////
	bool bSelected;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&sSector); f.Add(3,&bSelected); return 0; }

public:
	CGlobalSector() {}
	CGlobalSector( const SWindowInfo &sInfo, const SGlobalSector &sSector );

	virtual bool HitTest( int nX, int nY ) = 0;

	bool IsSelected() const;
	void SetSelected( bool bState );

	const SGlobalSector& GetSector() const;

	virtual void Update( const STime &sTime )= 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalSector::CGlobalSector( const SWindowInfo &sInfo, const SGlobalSector &_sSector ):
	CWindow( sInfo ), sSector( _sSector ), bSelected( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalSector::IsSelected() const
{
	return bSelected;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalSector::SetSelected( bool bState )
{
	bSelected = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SGlobalSector& CGlobalSector::GetSector() const
{
	return sSector;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalSector
////////////////////////////////////////////////////////////////////////////////////////////////////
class CZoneGlobalSector: public CGlobalSector
{
	OBJECT_BASIC_METHODS(CZoneGlobalSector)
private:
	ZDATA_(CGlobalSector)
	CPtr<NGame::IGlobalMap> pGlobal;
	////
	bool bRecommended;
	////
	float fCoeff;
	STime sMorphTime;
	STime sFlashTime;
	////
	CPtr<CImage> pZoneNormal;
	CPtr<CImage> pZoneDisabled;
	CPtr<CMLText> pTextNormal;
	CPtr<CMLText> pTextHilighted;
	CPtr<CWindow> pDescription;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CGlobalSector*)this); f.Add(2,&pGlobal); f.Add(3,&bRecommended); f.Add(4,&fCoeff); f.Add(5,&sMorphTime); f.Add(6,&sFlashTime); f.Add(7,&pZoneNormal); f.Add(8,&pZoneDisabled); f.Add(9,&pTextNormal); f.Add(10,&pTextHilighted); f.Add(11,&pDescription); return 0; }

protected:
	void UpdateSector();

public:
	CZoneGlobalSector() {}
	CZoneGlobalSector( const SWindowInfo &sInfo, NGame::IGlobalMap *pGlobal, const SGlobalSector &sSector, bool bRecommended );

	bool HitTest( int nX, int nY );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CZoneGlobalSector::CZoneGlobalSector( const SWindowInfo &sInfo, NGame::IGlobalMap *_pGlobal, const SGlobalSector &sSector, bool _bRecommended ):
	CGlobalSector( sInfo, sSector ), pGlobal( _pGlobal ), bRecommended( _bRecommended ), fCoeff( 0 ), sFlashTime( 0 ), sMorphTime( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZoneGlobalSector::HitTest( int nX, int nY )
{
	return IsPointInPolygon( GetSector().pointsSet, CVec2( nX, nY ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZoneGlobalSector::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pTextNormal = new CMLText( sEvent.pLoader->GetControl( "text_normal" ) );
			pTextHilighted = new CMLText( sEvent.pLoader->GetControl( "text_hilighted" ) );

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pZoneNormal = GetUIWindow<CImage>( this, "zone_normal" );
			pZoneDisabled = GetUIWindow<CImage>( this, "zone_disabled" );

			pDescription = GetUIWindow<CWindow>( this, "description" );

			break;
		}
	}

	return CGlobalSector::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CZoneGlobalSector::Update( const STime &sTime )
{
	float fTargetCoeff = IsSelected() ? 1.0f : 0.3f;
	if ( !IsSelected() && bRecommended )
	{
		fTargetCoeff = float( sTime % ( N_STANDART_FLASHTIME * 2 ) ) / N_STANDART_FLASHTIME;
		if ( fTargetCoeff > 1 )
			fTargetCoeff = 2 - fTargetCoeff;

		fTargetCoeff = fTargetCoeff * 0.5f + 0.3f;
	}

	if ( pGlobal->GetGlobalGame()->bChapterMapSet && ( pGlobal->GetMode() == NGame::IGlobalMap::MODE_SHOW ) && ( GetSector().nTemplate == pGlobal->GetGlobalGame()->nChapterMapID ) )
		fTargetCoeff = 1.0f;

	fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
	sMorphTime = sTime;

	pZoneNormal->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
	pZoneDisabled->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );

	pZoneNormal->SetStyle( STYLE_VISIBLE, pGlobal->GetMode() == NGame::IGlobalMap::MODE_NORMAL );
	pZoneDisabled->SetStyle( STYLE_VISIBLE, pGlobal->GetMode() == NGame::IGlobalMap::MODE_SHOW );

	pTextNormal->SetStyle( STYLE_VISIBLE, !IsSelected() );
	pTextHilighted->SetStyle( STYLE_VISIBLE, IsSelected() );

	pDescription->SetStyle( STYLE_VISIBLE, IsSelected() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalMapUI::CGlobalMapUI( const SWindowInfo &sInfo, NGame::IGlobalMap *_pGlobal ):
	CWindow( sInfo ), pGlobal( _pGlobal )
{
	sCursor = SCursorInfo( NDb::GetUITexture( 492 ) );

	CDGPtr<CPtrFuncBase<CGlobalInfo> > pGlobalInfo = pGlobal->GetGlobalInfo();
	pGlobalInfo.Refresh();
	pInfo = pGlobalInfo->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalMapUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( sCursor );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pReturn = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );
			pReturn->SetStyle( STYLE_VISIBLE, pGlobal->GetMode() == NGame::IGlobalMap::MODE_SHOW );

			NDb::CSide *pSide = pGlobal->GetGlobalPlayer()->pSide;
			CPtr<NDb::CUITexture> pBaseFlag, pBaseFlagActive;
			if ( IsValid( pSide ) )
			{
				pBaseFlag = pSide->pBaseFlag;
				pBaseFlagActive = pSide->pBaseFlagActive;
			}
			pBaseZone = new CFlashButton( sEvent.pLoader->GetControl( "basezone" ), pBaseFlag, pBaseFlagActive );
			pBaseZone->SetStyle( STYLE_VISIBLE, IsValid( pSide ) && ( pGlobal->GetMode() != NGame::IGlobalMap::MODE_SHOW ) );

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pMapView = GetUIWindow<CWindow>( this, "view" );

			pBackground = GetUIWindow<CImage>( this, "background" );
			pBackground->SetImage( pGlobal->GetGlobalMap()->pBackground );

			sectorsSet.reserve( pInfo->sectorsSet.size() );
			for ( int nTemp = 0; nTemp < pInfo->sectorsSet.size(); nTemp++ )
			{
				SGlobalSector &sGlobalSector = pInfo->sectorsSet[nTemp];
				if ( sGlobalSector.pointsSet.empty() )
					continue;

				bool bVisible = false, bRecommended = false;
				GetGlobalSectorInfo( sGlobalSector, &bVisible, &bRecommended );
				if ( bVisible )
				{
					SPoint sPosition;
					ScreenToClient( SPoint( sGlobalSector.vImagePos.x, sGlobalSector.vImagePos.y ), &sPosition );
					CGlobalSector* pSector = new CZoneGlobalSector( SWindowInfo( this, sPosition, SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TOPMOST | STYLE_TRANSPARENT ), pGlobal, sGlobalSector, bRecommended );
					LoadTemplate( pSector, NDb::GetUIContainer( sGlobalSector.nImageID ) );

					sectorsSet.push_back( pSector );
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
			if ( ( pGlobal->GetMode() == NGame::IGlobalMap::MODE_NORMAL ) && pMapView->HitTest( sEvent.nX, sEvent.nY ) )
			{
				for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
				{
					CGlobalSector *pSector = sectorsSet[nTemp];
					const SGlobalSector &sSector = pSector->GetSector();
					if ( pSector->HitTest( sEvent.nX, sEvent.nY ) )
					{
						NMainLoop::Command( new NGame::CICBeginChapter( sSector.nTemplate, pGlobal->GetGlobalGame() ) );
						break;
					}
				}
			}

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
void CGlobalMapUI::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	const SPoint &sCursorPos = GetInterface()->GetCursorPos();
	for ( int nTemp = 0; nTemp < sectorsSet.size(); nTemp++ )
	{
		CGlobalSector *pSector = sectorsSet[nTemp];
		pSector->SetSelected( false );
		if ( pSector->HitTest( sCursorPos.x, sCursorPos.y ) )
			pSector->SetSelected( true );

		pSector->Update( sTime );
	}

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapUI::GetGlobalSectorInfo( const SGlobalSector &sSector, bool *pbVisible, bool *pRecommended )
{
	*pbVisible = true;
	*pRecommended = false;

	if ( sSector.nTemplate == -1 )
		return;

	CPtr<NRPG::CGlobalGame> pGame = pGlobal->GetGlobalGame();

	*pbVisible = false;
	list<CPtr<NScenario::CScenarioZone> > zonesList;
	pGame->pScenarioTracker->GetAvailableZones( &zonesList );

	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo = shareChapterInfo.Get( sSector.nTemplate );
	pChapterInfo.Refresh();
	CObj<CChapterInfo> pInfo = pChapterInfo->GetValue();

	for( int nTemp = 0; nTemp < pInfo->sectorsSet.size(); nTemp++ )
	{
		const SChapterSector &sChapterSector = pInfo->sectorsSet[nTemp];
		if ( sChapterSector.eType != ZONE )
			continue;

		CPtr<NScenario::CScenarioZone> pZone = pGame->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( sChapterSector.nTemplate ) );
		if ( find( zonesList.begin(), zonesList.end(), pZone ) != zonesList.end() )
		{
			*pbVisible = true;
			if ( pGame->pScenarioTracker->GetRecommendedZone( pGame->players.front() ) == pZone )
				*pRecommended = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0912170, CGlobalMapUI );
REGISTER_SAVELOAD_CLASS( 0xB0912172, CZoneGlobalSector );
