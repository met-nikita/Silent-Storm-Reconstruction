#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "RPGUnit.h"
#include "RPGPerk.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataPerk.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iPerksPanel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerkButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPerkButton: public CButton
{
	OBJECT_BASIC_METHODS(CPerkButton)
public:
	enum EState
	{
		STATE_NORMAL,
		STATE_NORMAL_HOVER,
		STATE_DISABLED_TAKEN,
		STATE_DISABLED_TAKEN_HOVER,
		STATE_DISABLED_AVAILABLE,
		STATE_DISABLED_AVAILABLE_HOVER
	};
private:
	ZDATA_(CButton)
	CPtr<NRPG::CPerk> pPerk;
	////
	bool bTaken;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pPerk); f.Add(3,&bTaken); return 0; }

public:
	CPerkButton() {}
	CPerkButton( const SWindowInfo &sInfo, NRPG::CPerk *pPerk );

	void Set( bool _bTaken );
	NRPG::CPerk* GetPerk();

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerkButton::CPerkButton( const SWindowInfo &sInfo, NRPG::CPerk *_pPerk ):
	CButton( sInfo ), pPerk( _pPerk )
{
	NDb::CDBPerk *pDBPerk = pPerk->GetDBPerk();
	AddImageState( STATE_NORMAL, pDBPerk->pIcon );
	AddImageState( STATE_NORMAL_HOVER, pDBPerk->pIcon, NGfx::SPixel8888( 0x7F, 0x7F, 0xFF, 0xFF ) );
	AddImageState( STATE_DISABLED_TAKEN, pDBPerk->pIcon );
	AddImageState( STATE_DISABLED_TAKEN_HOVER, pDBPerk->pIcon, NGfx::SPixel8888( 0x7F, 0x7F, 0xFF, 0xFF ) );
	AddImageState( STATE_DISABLED_AVAILABLE, pDBPerk->pIconDisabled );
	AddImageState( STATE_DISABLED_AVAILABLE_HOVER, pDBPerk->pIconDisabled, NGfx::SPixel8888( 0x7F, 0x7F, 0xFF, 0xFF ) );

	CPtr<CToolTip> pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	pToolTip->SetText( GetDBString( pDBPerk->pToolTip ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerkButton::Set( bool _bTaken )
{
	bTaken = _bTaken;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CPerk* CPerkButton::GetPerk()
{
	return pPerk;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerkButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !GetStyle( STYLE_ENABLED ) )
	{
		if ( bTaken )
			SetActiveState( IsMouseCover() ? STATE_DISABLED_TAKEN_HOVER : STATE_DISABLED_TAKEN );
		else
			SetActiveState( IsMouseCover() ? STATE_DISABLED_AVAILABLE_HOVER : STATE_DISABLED_AVAILABLE );
	}
	else
		SetActiveState( IsMouseCover() ? STATE_NORMAL_HOVER : STATE_NORMAL );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerksPanelView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPerksPanelView: public CWindow
{
	OBJECT_BASIC_METHODS(CPerksPanelView)
private:
	ZDATA_(CWindow)
	CPtr<NWorld::CUnit> pUnit;
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CButton> pClose;
	CObj<CMLText> pTextInfo;
	vector<CPtr<CPerkButton> > perksButtons;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pUnit); f.Add(3,&pMission); f.Add(4,&pClose); f.Add(5,&pTextInfo); f.Add(6,&perksButtons); return 0; }

public:
	CPerksPanelView() {}
	CPerksPanelView( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::CUnit *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerksPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerksPanelView::CPerksPanelView( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::CUnit *_pUnit ):
	CWindow( sInfo ), pMission( _pMission ), pUnit( _pUnit )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPerksPanelView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			for ( int nTemp = 0; nTemp < perksButtons.size(); nTemp++ )
			{
				CPerkButton *pButton = perksButtons[nTemp];
				if ( pButton->GetWindowID() == sEvent.szID )
				{
					pMission->Command( pUnit, new NWorld::CCmdTakePerk( pButton->GetPerk()->GetDBPerk()->GetRecordID() ) );
					return true;
				}
			}
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pTextInfo = new CMLText( sEvent.pLoader->GetControl( "info" ) );

			NRPG::CPerksTree *pTree = pUnit->GetRPG()->GetRPGUnit()->GetPerksTree();

			vector<CPtr<NRPG::CPerk> > perksSet;
			pTree->GetAllPerks( &perksSet );

			perksButtons.resize( perksSet.size() );
			for( int nTemp = 0; nTemp < perksSet.size(); nTemp++ )
			{
				NRPG::CPerk *pPerk = perksSet[nTemp];
				NDb::CDBPerk *pDBPerk = pPerk->GetDBPerk();
				perksButtons[nTemp] = new CPerkButton( sEvent.pLoader->GetControl( pDBPerk->szUserName ), pPerk );
			}

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pClose = GetUIWindow<CButton>( this, "perks" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksPanelView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	NRPG::CPerksTree *pTree = pUnit->GetRPG()->GetRPGUnit()->GetPerksTree();

	pTextInfo->SetVal( L"points", pTree->GetPerkPoints() );

	vector<CPtr<NRPG::CPerk> > takenPerksSet;
	pTree->GetTakenPerks( &takenPerksSet );

	vector<CPtr<NRPG::CPerk> > availablePerksSet;
	pTree->GetAvailablePerks( &availablePerksSet );

	for ( int nTemp = 0; nTemp < perksButtons.size(); nTemp++ )
	{
		CPerkButton *pButton = perksButtons[nTemp];
		if ( find( takenPerksSet.begin(), takenPerksSet.end(), pButton->GetPerk() ) != takenPerksSet.end() )
			pButton->Set( true );
		else
			pButton->Set( false );

		bool bEnabled = find( availablePerksSet.begin(), availablePerksSet.end(), pButton->GetPerk() ) != availablePerksSet.end();
		pButton->SetStyle( STYLE_ENABLED, bEnabled );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerksPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerksPanel::CPerksPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPerksPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pBaseView = GetUIWindow<CWindow>( this, "view" );
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
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
void CPerksPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( ( unitsSet.size() == 1 ) && ( pUnit != unitsSet.front()->GetUnit() ) )
	{
		pUnit = unitsSet.front()->GetUnit();
		pPerksView = new CPerksPanelView( SWindowInfo( pBaseView, SPoint( 0, 0 ), pBaseView->GetSize(), "view", STYLE_ENABLED | STYLE_VISIBLE ), pMission, pUnit );

		if ( IsValid( pUnit->GetRPG()->GetRPGPers()->pClass ) )
			LoadTemplate( pPerksView, pUnit->GetRPG()->GetRPGPers()->pClass->pPerksPanel );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1202130, CPerksPanel );
REGISTER_SAVELOAD_CLASS( 0xB1202131, CPerksPanelView );
