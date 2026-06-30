// AnimItemsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "AnimItemsDlg.h"
#include "dbDefs.h"
#include "TreeSelItemDlg.h"
#include "ItemsMgr.h"


extern vector<pair<string, int> > gvAnimItems;
extern int gnHeadID;
////////////////////////////////////////////////////////////////////////////////////////////////////
string szEffectorNames[N_EFFECTORS] = 
{
	"Slot1",
	"Slot2",
	"Slot3",
	"Slot4",
	"Slot5",
	"Slot6",
	"Slot7",
	"Slot8",
	"Slot9",
	"Cap",
	"BackPack",
	"Rifle",
	"SubMachineGun",
	"MachineGun",
	"RocketLauncher",
	"Item",
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ReadEffectorPrefs()
{
	for ( int i = 0; i < N_EFFECTORS; ++i )
	{
		int n = theApp.GetProfileInt( "Effectors", szEffectorNames[i].c_str(), -1 );
		gvAnimItems.push_back( pair<string, int>( szEffectorNames[i], n ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteEffectorPrefs()
{
	for ( int i = 0; i < gvAnimItems.size(); ++i )
		theApp.WriteProfileInt( "Effectors", gvAnimItems[i].first.c_str(), gvAnimItems[i].second );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimItemsDlg dialog
IMPLEMENT_DYNAMIC(CAnimItemsDlg, CDialog)
CAnimItemsDlg::CAnimItemsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAnimItemsDlg::IDD, pParent)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimItemsDlg::~CAnimItemsDlg()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimItemsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANIM_HEAD, m_head);
	DDX_Control(pDX, IDC_ANIM_ITEM, m_item);
	DDX_Control(pDX, IDC_ANIM_CAP, m_cap);
	DDX_Control(pDX, IDC_ANIM_BACKPACK, m_backpack);
	DDX_Control(pDX, IDC_ANIM_RIFLE, m_rifle);
	DDX_Control(pDX, IDC_ANIM_SMG, m_smg);
	DDX_Control(pDX, IDC_ANIM_MG, m_mg);
	DDX_Control(pDX, IDC_ANIM_RL, m_rl);
	DDX_Control(pDX, IDC_SLOT1, m_slot1);
	DDX_Control(pDX, IDC_SLOT2, m_slot2);
	DDX_Control(pDX, IDC_SLOT3, m_slot3);
	DDX_Control(pDX, IDC_SLOT4, m_slot4);
	DDX_Control(pDX, IDC_SLOT5, m_slot5);
	DDX_Control(pDX, IDC_SLOT6, m_slot6);
	DDX_Control(pDX, IDC_SLOT7, m_slot7);
	DDX_Control(pDX, IDC_SLOT8, m_slot8);
	DDX_Control(pDX, IDC_SLOT9, m_slot9);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CAnimItemsDlg, CDialog)
	ON_BN_CLICKED(IDC_ANIM_HEAD, OnBnClickedAnimHead)
	ON_BN_CLICKED(IDC_ANIM_ITEM, OnBnClickedAnimItem)
	ON_BN_CLICKED(IDC_ANIM_CAP, OnBnClickedAnimCap)
	ON_BN_CLICKED(IDC_ANIM_BACKPACK, OnBnClickedAnimBackpack)
	ON_BN_CLICKED(IDC_ANIM_RIFLE, OnBnClickedAnimRifle)
	ON_BN_CLICKED(IDC_ANIM_SMG, OnBnClickedAnimSmg)
	ON_BN_CLICKED(IDC_ANIM_MG, OnBnClickedAnimMg)
	ON_BN_CLICKED(IDC_ANIM_RL, OnBnClickedAnimRl)
	ON_BN_CLICKED(IDC_SLOT1, OnBnClickedSlot1)
	ON_BN_CLICKED(IDC_SLOT2, OnBnClickedSlot2)
	ON_BN_CLICKED(IDC_SLOT3, OnBnClickedSlot3)
	ON_BN_CLICKED(IDC_SLOT4, OnBnClickedSlot4)
	ON_BN_CLICKED(IDC_SLOT5, OnBnClickedSlot5)
	ON_BN_CLICKED(IDC_SLOT6, OnBnClickedSlot6)
	ON_BN_CLICKED(IDC_SLOT7, OnBnClickedSlot7)
	ON_BN_CLICKED(IDC_SLOT8, OnBnClickedSlot8)
	ON_BN_CLICKED(IDC_SLOT9, OnBnClickedSlot9)
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CAnimItemsDlg::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return FALSE;
	vAnimItems = gvAnimItems;
	const SResTree *pTree = theApp.GetResTree( IDC_MODELS_TREE );
	const SResTree *pHeadTree = theApp.GetResTree( IDC_COMPLEXHEADS );
	if ( !pTree || !pHeadTree )
		return FALSE;
	CButton* btns[N_EFFECTORS] =
	{
		&m_slot1,
		&m_slot2,
		&m_slot3,
		&m_slot4,
		&m_slot5,
		&m_slot6,
		&m_slot7,
		&m_slot8,
		&m_slot9,
		&m_cap,
		&m_backpack,
		&m_rifle,
		&m_smg,
		&m_mg,
		&m_rl,
		&m_item
	};
	for ( int i = 0; i < vAnimItems.size(); ++i )
	{
		int it = 0;
		for ( ; it < N_EFFECTORS; ++it )
			if ( vAnimItems[i].first == szEffectorNames[it] )
				break;
		if ( it != N_EFFECTORS )
			btns[it]->SetWindowText( pTree->pItemsTree->GetItemPath( vAnimItems[i].second ).c_str() );
	}
	nHeadID = gnHeadID;
	m_head.SetWindowText( pHeadTree->pItemsTree->GetItemPath( nHeadID ).c_str() );
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimItemsDlg::OnOK()
{
	gvAnimItems = vAnimItems;
	gnHeadID = nHeadID;
	WriteEffectorPrefs();
	theApp.WriteProfileInt( "", "HeadID", gnHeadID );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimItemsDlg::SelectModel( const string &szEffector, CButton *pBtn )
{
	const SResTree *pTree = theApp.GetResTree( IDC_MODELS_TREE );
	if ( !pTree )
		return;
	int nModelID = -1;
	vector<pair<string, int> >::iterator it = vAnimItems.begin();
	for ( ; it != vAnimItems.end(); ++it )
		if ( it->first == szEffector )
		{
			nModelID = it->second;
			break;
		}
	pTree->pTreeDlg->SetSelectedItemID( IDC_MODELS_TREE, nModelID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nModelID );
	if ( it != vAnimItems.end() )
	{
		it->second = nModelID;
	}
	else
	{
		vAnimItems.push_back( pair<string, int>( szEffector, nModelID ) );
	}
	if ( pBtn && pTree->pItemsTree )
		pBtn->SetWindowText( pTree->pItemsTree->GetItemPath( nModelID ).c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimItemsDlg message handlers
void CAnimItemsDlg::OnBnClickedAnimHead()
{
	const SResTree *pTree = theApp.GetResTree( IDC_COMPLEXHEADS );
	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_COMPLEXHEADS, nHeadID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nHeadID );
	if ( m_head && pTree->pItemsTree )
		m_head.SetWindowText( pTree->pItemsTree->GetItemPath( nHeadID ).c_str() );
}

void CAnimItemsDlg::OnBnClickedAnimItem()
{
	SelectModel( "Item", &m_item );
}

void CAnimItemsDlg::OnBnClickedAnimCap()
{
	SelectModel( "Cap", &m_cap );
}

void CAnimItemsDlg::OnBnClickedAnimBackpack()
{
	SelectModel( "BackPack", &m_backpack );
}

void CAnimItemsDlg::OnBnClickedAnimRifle()
{
	SelectModel( "Rifle", &m_rifle );
}

void CAnimItemsDlg::OnBnClickedAnimSmg()
{
	SelectModel( "SubMachineGun", &m_smg );
}

void CAnimItemsDlg::OnBnClickedAnimMg()
{
	SelectModel( "MachineGun", &m_mg );
}

void CAnimItemsDlg::OnBnClickedAnimRl()
{
	SelectModel( "RocketLauncher", &m_rl );
}

void CAnimItemsDlg::OnBnClickedSlot1()
{
	SelectModel( "Slot1", &m_slot1 );
}

void CAnimItemsDlg::OnBnClickedSlot2()
{
	SelectModel( "Slot2", &m_slot2 );
}

void CAnimItemsDlg::OnBnClickedSlot3()
{
	SelectModel( "Slot3", &m_slot3 );
}

void CAnimItemsDlg::OnBnClickedSlot4()
{
	SelectModel( "Slot4", &m_slot4 );
}

void CAnimItemsDlg::OnBnClickedSlot5()
{
	SelectModel( "Slot5", &m_slot5 );
}

void CAnimItemsDlg::OnBnClickedSlot6()
{
	SelectModel( "Slot6", &m_slot6 );
}

void CAnimItemsDlg::OnBnClickedSlot7()
{
	SelectModel( "Slot7", &m_slot7 );
}

void CAnimItemsDlg::OnBnClickedSlot8()
{
	SelectModel( "Slot8", &m_slot8 );
}

void CAnimItemsDlg::OnBnClickedSlot9()
{
	SelectModel( "Slot9", &m_slot9 );
}
