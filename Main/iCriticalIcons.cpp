#include "StdAfx.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "iMission.h"
#include "Interface.h"
#include "UIInterface.h"
#include "iCriticalIcons.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelCritical::CInfoPanelCritical( const SWindowInfo &sInfo ):
	CImage( sInfo )
{
	pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelCritical::Set( NRPG::ICriticalInfo *_pCritical )
{
	if ( pCritical == _pCritical )
		return;

	pCritical = _pCritical;

	// release @0x1cd370: the icon table was renumbered vs the dev tree (textures 810..827, not
	// 604..617); a TEMPORARY critical (IsTemporarily) shows the lighter sibling icon texBase-1;
	// C_ENCUMBRANCE / C_ACCIDENTAL_SHOT / C_LOST_WEAPON / C_DAMAGE_WEAPON lost their icons; the
	// "difficulty" tooltip value moved into Draw (refreshed per frame together with critdur).
	int nTexBase = 0, nStringId = 0;
	bool bVariant = false, bHasIcon = true;
	switch ( pCritical->GetCriticalType() )
	{
	case NDb::C_AP_REDUCTION:          nTexBase = 811; nStringId = 11177; bVariant = true;  break;
	case NDb::C_BLIND:                 nTexBase = 814; nStringId = 11178; bVariant = true;  break;
	case NDb::C_WEAPONSKILL_REDUCTION: nTexBase = 823; nStringId = 11179; bVariant = true;  break;
	case NDb::C_VP:                    nTexBase = 827; nStringId = 11180; bVariant = true;  break;
	case NDb::C_MOTIONLESS:            nTexBase = 818; nStringId = 11181; bVariant = true;  break;
	case NDb::C_IDLE_HAND:             nTexBase = 825; nStringId = 11185; bVariant = true;  break;
	case NDb::C_STUN:                  nTexBase = 821; nStringId = 11186; bVariant = true;  break;
	case NDb::C_PATIENT:               nTexBase = 819; nStringId = 11188; bVariant = false; break;
	case NDb::C_DEAF:                  nTexBase = 816; nStringId = 11189; bVariant = true;  break;
	case NDb::C_BLEEDING:              nTexBase = 812; nStringId = 11190; bVariant = false; break;
	default:                           bHasIcon = false; break; // no icon in release
	}
	if ( bHasIcon )
	{
		int nTexture = nTexBase;
		if ( bVariant && pCritical->IsTemporarily() )
			nTexture = nTexBase - 1;
		SetImage( NDb::GetUITexture( nTexture ) );
		pToolTip->SetText( GetDBString( nStringId ) );
	}

	switch( pCritical->GetCriticalLocation() )
	{
	case NDb::CL_HEAD:
		pToolTip->SetVal( L"location", GetDBString( 11191 ) );
		break;
	case NDb::CL_TORSO:
		pToolTip->SetVal( L"location", GetDBString( 11192 ) );
		break;
	case NDb::CL_ARMS:
		pToolTip->SetVal( L"location", GetDBString( 11193 ) );
		break;
	case NDb::CL_LEGS:
		pToolTip->SetVal( L"location", GetDBString( 11194 ) );
		break;
	case NDb::CL_ANY:
		pToolTip->SetVal( L"location", GetDBString( 11195 ) );
		break;
	// release @0x1cd370 has no default case (out-of-range location sets nothing)
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelCritical::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// release @0x1cd040: refresh the live tooltip values every frame; DB string 19810 is the
	// generic "n/a" fallback for non-positive durations / difficulty classes.
	pToolTip->SetVal( L"bleedspeed", pCritical->GetValue() );

	const int nDuration = pCritical->GetRemainingTime();
	if ( nDuration > 0 )
		pToolTip->SetVal( L"critdur", nDuration );
	else
		pToolTip->SetVal( L"critdur", GetDBString( 19810 ) );

	const int nDC = pCritical->GetDifficultyClass();
	if ( nDC > 0 )
		pToolTip->SetVal( L"difficulty", nDC );
	else
		pToolTip->SetVal( L"difficulty", GetDBString( 19810 ) );

	CImage::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateCriticalIcons
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateCriticalIcons( NGame::IUnitTracker *pUnit, const vector<CObj<CInfoPanelCritical> > &criticalIconsSet )
{
	// release @0x1cda90
	list<CPtr<NRPG::ICriticalInfo> > criticalsList;
	pUnit->GetUnit()->GetRPG()->GetCriticalsList( &criticalsList );

	// mission-scripted bleeding (release CUnitServer::GetBleeding @0x3c68d0) draws as a
	// permanent fake bleeding icon on top of the real criticals
	const int nBleeding = pUnit->GetUnit()->GetBleeding();
	if ( nBleeding > 0 )
		criticalsList.push_back( new CMissionCriticalBleedingFake( nBleeding ) );

	int nCount = 0;
	for ( list<CPtr<NRPG::ICriticalInfo> >::const_iterator iTemp = criticalsList.begin(); iTemp != criticalsList.end(); ++iTemp )
	{
		// PK statuses (everything past the plain-wound block) get no icon slot (release: type < 0x10)
		if ( (*iTemp)->GetCriticalType() >= NDb::C_PANZERKLEIN_AXIS )
			continue;

		if ( nCount >= criticalIconsSet.size() )
			break;

		CInfoPanelCritical *pIcon = criticalIconsSet[nCount];
		pIcon->Set( *iTemp );
		pIcon->SetStyle( STYLE_VISIBLE, true );

		nCount++;
	}
	for ( int nTemp = nCount; nTemp < criticalIconsSet.size(); nTemp++ )
		criticalIconsSet[nTemp]->SetStyle( STYLE_VISIBLE, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241944, CInfoPanelCritical );
REGISTER_SAVELOAD_CLASS( 0xB3515150, CMissionCriticalBleedingFake );
