// RPGItemsContainerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "RPGItemsContainerDlg.h"
#include "dbDefs.h"
#include "..\DBFormat\DataRPGTmp.h"

namespace NDb
{
	void GiveItems();
}
static void UpdateDB()
{
	Sleep(0);
	NDatabase::Refresh<NDb::CRPGPers>();
	NDatabase::Refresh<NDb::CRPGWeapon4Pers>();
	NDatabase::Refresh<NDb::CRPGClip4Pers>();
	NDatabase::Refresh<NDb::CRPGGrenade4Pers>();
	NDatabase::Refresh<NDb::CRPGFirstAid4Pers>();
	NDatabase::Refresh<NDb::CRPGMineDetector4Pers>();
	NDatabase::Refresh<NDb::CRPGMeleeWeapon4Pers>();
	NDatabase::Refresh<NDb::CRPGMine4Pers>();
	NDatabase::Refresh<NDb::CRPGTool4Pers>();
	NDatabase::Refresh<NDb::CRPGKey4Pers>();
	NDb::GiveItems();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsContainerDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CRPGItemsContainerDlg, CPropertySheet)
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGItemsContainerDlg::CRPGItemsContainerDlg( int nRPGPersID, CWnd* pParent /*=NULL*/)
	: CPropertySheet(),
	m_ctrlWeapons( IDS_WEAPONS, nRPGPersID, IDC_RPG_WEAPONS_TREE, "RPGWeapon4Pers" ),
	m_ctrlClips( IDS_CLIPS, nRPGPersID, IDC_RPG_CLIPS_TREE, "RPGClip4Pers" ),
	m_ctrlGrenades( IDS_GRENADES, nRPGPersID, IDC_RPG_GRENADES_TREE, "RPGGrenade4Pers" ),
	m_ctrlFirstAids( IDS_FIRSTAID, nRPGPersID, IDC_RPG_FIRSTAID_TREE, "RPGFirstAid4Pers" ),
	m_ctrlMineDetectors( IDS_MINEDETECTORS, nRPGPersID, IDC_RPG_MINEDETECTORS_TREE, "RPGMineDetector4Pers" ),
	m_ctrlMelee( IDS_MELEE, nRPGPersID, IDC_RPG_MELEEWEAPONS_TREE, "RPGMeleeWeapon4Pers" )
{
	AddPage( &m_ctrlWeapons );
	AddPage( &m_ctrlClips );
	AddPage( &m_ctrlGrenades );
	AddPage( &m_ctrlFirstAids );
	AddPage( &m_ctrlMineDetectors );
	AddPage( &m_ctrlMelee );

	SetTitle( "Items" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemsContainerDlg::SetRPGPersID( int nPersID )
{
	m_ctrlWeapons.SetRPGPersID( nPersID );
	m_ctrlClips.SetRPGPersID( nPersID );
	m_ctrlGrenades.SetRPGPersID( nPersID );
	m_ctrlFirstAids.SetRPGPersID( nPersID );
	m_ctrlMineDetectors.SetRPGPersID( nPersID );
	m_ctrlMelee.SetRPGPersID( nPersID );

	UpdateDB();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGItemsContainerDlg::~CRPGItemsContainerDlg()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CRPGItemsContainerDlg, CPropertySheet)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsContainerDlg message handlers
////////////////////////////////////////////////////////////////////////////////////////////////////
