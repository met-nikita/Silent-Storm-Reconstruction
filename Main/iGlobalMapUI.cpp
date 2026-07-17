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
// CFrameText -- retail NUI::CFrameText (iGlobalMapUI.obj, saveload id 0xB0912171): a CFrame-derived
// bordered panel that owns one auto-sizing CText caption, (re)built from the originating UI
// control's DB string on EVENT_TEMPLATECREATE (ProcessMessage @0x1e4770; ctors @0x1e5ff0/@0x1e4740).
// NOTE: retail defines NO operator& of its own -- serialization is the inherited CFrame table
// (rel_tag_count 0 in the save-format audit); pText is deliberately NOT serialized (it is rebuilt
// by the template-create event), so none is authored here either. Registered for save-graph
// identity. Retail creation site: CZoneGlobalSector::pDescription (that retag is a separate
// convergence leg; until it lands the class is registration+behaviour only).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFrameText: public CFrame
{
	OBJECT_NOCOPY_METHODS(CFrameText);
private:
	CObj<CText> pText;   // the caption child; NOT in the save format (see banner)

public:
	CFrameText() {}
	CFrameText( const SWindowInfo &sInfo ): CFrame( sInfo ) {}   // retail @0x1e4740: CFrame(sInfo); pText = 0

	// retail @0x1e4770: on TEMPLATECREATE build the caption from the control's DB string and
	// auto-fit caption + frame heights (child inset {4,4}, width = frame width - 8, style 6 =
	// VISIBLE|ENABLED; frame height = measured text height + 8). Everything else -> CWindow.
	bool ProcessMessage( const SEvent &sEvent )
	{
		if ( sEvent.nEvent == EVENT_TEMPLATECREATE )
		{
			pText = new CText( SWindowInfo( this, SPoint( 4, 4 ), SPoint( GetSize().x - 8, 0 ), "", STYLE_VISIBLE | STYLE_ENABLED ) );
			pText->SetText( GetDBString( sEvent.pControl->pString ), true );

			SPoint sReal;
			pText->GetRealSize( &sReal );
			pText->SetSize( SPoint( pText->GetSize().x, sReal.y ) );   // caption: keep width, fit height
			SetSize( SPoint( GetSize().x, sReal.y + 8 ) );             // frame: text height + 4+4 border
		}

		return CWindow::ProcessMessage( sEvent );
	}
};
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
	CPtr<CText> pTextNormal;
	CPtr<CText> pTextHilighted;
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
			pTextNormal = new CText( sEvent.pLoader->GetControl( "text_normal" ) );
			pTextHilighted = new CText( sEvent.pLoader->GetControl( "text_hilighted" ) );

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
	// retail CGlobalMapUI ctor @0x1e4b20: disasm @0x5e4bfb `mov ecx,1` -> NDb::GetUICursor(1) =
	// UICursors row 1 "xz" (UITexture 295, NormalPen.cur) -- the global-map default cursor.
	sCursor = SCursorInfo( NDb::GetUICursor( 1 ) );

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
REGISTER_SAVELOAD_CLASS( 0xB0912171, CFrameText );     // retail NUI::CFrameText id (gen/classreg.json)
REGISTER_SAVELOAD_CLASS( 0xB0912172, CZoneGlobalSector );
