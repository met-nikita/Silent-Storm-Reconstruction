// AnimFlagsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "AnimFlagsDlg.h"
#include "ItemsMgr.h"
#include "dbDefs.h"


// CAnimFlagsDlg dialog

IMPLEMENT_DYNAMIC(CAnimFlagsDlg, CDialog)
CAnimFlagsDlg::CAnimFlagsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAnimFlagsDlg::IDD, pParent)
	, bPoseStand(FALSE)
	, bPoseCrouch(FALSE)
	, bPoseCrawl(FALSE)
	, bWeaponNoWeapon(FALSE)
	, bWeaponItem(FALSE)
	, bWeaponPistol(FALSE)
	, bWeaponRifle(FALSE)
	, bWeaponSubMachineGun(FALSE)
	, bWeaponMachineGun(FALSE)
	, bWeaponRLauncher(FALSE)
	, bWeaponKnife(FALSE)
	, bWeaponMachete(FALSE)
	, bWeaponKatana(FALSE)
	, bWeaponMDetector(FALSE)
	, bWeaponPKShooter(FALSE)
	, bWeaponPKSlasher(FALSE)
	, bWeaponPKRepairer(FALSE)
	, bMale(FALSE)
	, bFemale(FALSE)
	, bCombat(FALSE)
	, bRealtime(FALSE)
	, bClassEngineer(FALSE)
	, bClassGrenadier(FALSE)
	, bClassMedic(FALSE)
	, bClassScout(FALSE)
	, bClassSniper(FALSE)
	, bClassSoldier(FALSE)
	, bClassEnemy(FALSE)
	, bWeaponPlazmaGun(FALSE)
	, bWeaponPKPlazmaGun(FALSE)
{
	nAnimationID = -1;
}

CAnimFlagsDlg::~CAnimFlagsDlg()
{
}

void CAnimFlagsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_POSE_STAND, bPoseStand);
	DDX_Check(pDX, IDC_POSE_CROUCH, bPoseCrouch);
	DDX_Check(pDX, IDC_POSE_CRAWL, bPoseCrawl);
	DDX_Check(pDX, IDC_WEAPON_NoWeapon, bWeaponNoWeapon);
	DDX_Check(pDX, IDC_WEAPON_Item, bWeaponItem);
	DDX_Check(pDX, IDC_WEAPON_Pistol, bWeaponPistol);
	DDX_Check(pDX, IDC_WEAPON_Rifle, bWeaponRifle);
	DDX_Check(pDX, IDC_WEAPON_SubMachineGun, bWeaponSubMachineGun);
	DDX_Check(pDX, IDC_WEAPON_MachineGun, bWeaponMachineGun);
	DDX_Check(pDX, IDC_WEAPON_RLauncher, bWeaponRLauncher);
	DDX_Check(pDX, IDC_WEAPON_Knife, bWeaponKnife);
	DDX_Check(pDX, IDC_WEAPON_Machete, bWeaponMachete);
	DDX_Check(pDX, IDC_WEAPON_Katana, bWeaponKatana);
	DDX_Check(pDX, IDC_WEAPON_MDetector, bWeaponMDetector);
	DDX_Check(pDX, IDC_WEAPON_PKShooter, bWeaponPKShooter);
	DDX_Check(pDX, IDC_WEAPON_PKSlasher, bWeaponPKSlasher);
	DDX_Check(pDX, IDC_WEAPON_PKRepairer, bWeaponPKRepairer);
	DDX_Check(pDX, IDC_MALE, bMale);
	DDX_Check(pDX, IDC_FEMALE, bFemale);
	DDX_Check(pDX, IDC_COMBAT, bCombat);
	DDX_Check(pDX, IDC_REALTIME, bRealtime);
	DDX_Check(pDX, IDC_CLASS_Engineer, bClassEngineer);
	DDX_Check(pDX, IDC_CLASS_Grenadier, bClassGrenadier);
	DDX_Check(pDX, IDC_CLASS_Medic, bClassMedic);
	DDX_Check(pDX, IDC_CLASS_Scout, bClassScout);
	DDX_Check(pDX, IDC_CLASS_Sniper, bClassSniper);
	DDX_Check(pDX, IDC_CLASS_Soldier, bClassSoldier);
	DDX_Check(pDX, IDC_CLASS_Enemy, bClassEnemy);
	DDX_Check(pDX, IDC_WEAPON_PLAZMAGUN, bWeaponPlazmaGun);
	DDX_Check(pDX, IDC_WEAPON_PK_PlazmaGun, bWeaponPKPlazmaGun);
}

inline bool GetFlag( const CPropMap *pProps, const string &szFlag )
{
	CPropMap::const_iterator i = pProps->find( szFlag );
	if ( i != pProps->end() )
		return i->second->GetValue();
	return false;
}

void CAnimFlagsDlg::SetAnimationID( int nID )
{
	const SResTree *pTree = theApp.GetResTree( IDC_ANIMATIONS_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	nAnimationID = nID;

	bPoseStand = GetFlag( pProps, "Stand" );
	bPoseCrouch = GetFlag( pProps, "Crouch" );
	bPoseCrawl = GetFlag( pProps, "Crawl" );

	bWeaponNoWeapon = GetFlag( pProps, "NoWeapon" );
	bWeaponItem = GetFlag( pProps, "Item" );
	bWeaponPistol = GetFlag( pProps, "Pistol" );
	bWeaponRifle = GetFlag( pProps, "Rifle" );
	bWeaponSubMachineGun = GetFlag( pProps, "SubMachineGun" );
	bWeaponMachineGun = GetFlag( pProps, "MachineGun" );
	bWeaponRLauncher = GetFlag( pProps, "RLauncher" );
	bWeaponKnife = GetFlag( pProps, "Knife" );
	bWeaponMachete = GetFlag( pProps, "Machete" );
	bWeaponKatana = GetFlag( pProps, "Katana" );
	bWeaponMDetector = GetFlag( pProps, "MDetector" );
	bWeaponPKShooter = GetFlag( pProps, "PKShooter" );
	bWeaponPKSlasher = GetFlag( pProps, "PKSlasher" );
	bWeaponPKRepairer = GetFlag( pProps, "PKRepairer" );
	bWeaponPKPlazmaGun = GetFlag( pProps, "PKPlazmagun" );
	bWeaponPlazmaGun = GetFlag( pProps, "Plazmagun" );

	bMale = GetFlag( pProps, "Male" );
	bFemale = GetFlag( pProps, "Female" );

	bCombat = GetFlag( pProps, "Combat" );
	bRealtime = GetFlag( pProps, "Realtime" );

	bClassEngineer = GetFlag( pProps, "Engineer" );
	bClassGrenadier = GetFlag( pProps, "Grenadier" );
	bClassMedic = GetFlag( pProps, "Medic" );
	bClassScout = GetFlag( pProps, "Scout" );
	bClassSniper = GetFlag( pProps, "Sniper" );
	bClassSoldier = GetFlag( pProps, "Soldier" );
	bClassEnemy = GetFlag( pProps, "Enemy" );

	pTree->pItemsTree->ReleasePropList( pProps );
	if ( ::IsWindow( m_hWnd ) )
		UpdateData( FALSE );
}

void CAnimFlagsDlg::UpdateItem( int nID )
{
	CButton *pB = (CButton*)GetDlgItem( nID );
	if ( !pB )
		return;
	//
	const SResTree *pTree = theApp.GetResTree( IDC_ANIMATIONS_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nAnimationID );
	if ( !pProps )
		return;
	//
	int nCheck = pB->GetCheck();
	CString szText;
	pB->GetWindowText( szText );
	CPropMap::const_iterator i = pProps->find( (LPCSTR)szText );
	if ( i != pProps->end() )
	{
		i->second->SetValue( nCheck > 0 );
	}
	//
	pTree->pItemsTree->ReleasePropList( pProps );
}

BEGIN_MESSAGE_MAP(CAnimFlagsDlg, CDialog)
	ON_BN_CLICKED(IDC_POSE_STAND, OnBnClickedPoseStand)
	ON_BN_CLICKED(IDC_POSE_CROUCH, OnBnClickedPoseCrouch)
	ON_BN_CLICKED(IDC_POSE_CRAWL, OnBnClickedPoseCrawl)
	ON_BN_CLICKED(IDC_WEAPON_NoWeapon, OnBnClickedWeaponNoweapon)
	ON_BN_CLICKED(IDC_WEAPON_Item, OnBnClickedWeaponItem)
	ON_BN_CLICKED(IDC_WEAPON_Pistol, OnBnClickedWeaponPistol)
	ON_BN_CLICKED(IDC_WEAPON_Rifle, OnBnClickedWeaponRifle)
	ON_BN_CLICKED(IDC_WEAPON_SubMachineGun, OnBnClickedWeaponSubmachinegun)
	ON_BN_CLICKED(IDC_WEAPON_MachineGun, OnBnClickedWeaponMachinegun)
	ON_BN_CLICKED(IDC_WEAPON_RLauncher, OnBnClickedWeaponRlauncher)
	ON_BN_CLICKED(IDC_WEAPON_Knife, OnBnClickedWeaponKnife)
	ON_BN_CLICKED(IDC_WEAPON_Machete, OnBnClickedWeaponMachete)
	ON_BN_CLICKED(IDC_WEAPON_Katana, OnBnClickedWeaponKatana)
	ON_BN_CLICKED(IDC_WEAPON_MDetector, OnBnClickedWeaponMdetector)
	ON_BN_CLICKED(IDC_WEAPON_PKShooter, OnBnClickedWeaponPkshooter)
	ON_BN_CLICKED(IDC_WEAPON_PKSlasher, OnBnClickedWeaponPkslasher)
	ON_BN_CLICKED(IDC_WEAPON_PKRepairer, OnBnClickedWeaponPkrepairer)
	ON_BN_CLICKED(IDC_MALE, OnBnClickedMale)
	ON_BN_CLICKED(IDC_FEMALE, OnBnClickedFemale)
	ON_BN_CLICKED(IDC_COMBAT, OnBnClickedCombat)
	ON_BN_CLICKED(IDC_REALTIME, OnBnClickedRealtime)
	ON_BN_CLICKED(IDC_CLASS_Engineer, OnBnClickedClassEngineer)
	ON_BN_CLICKED(IDC_CLASS_Grenadier, OnBnClickedClassGrenadier)
	ON_BN_CLICKED(IDC_CLASS_Medic, OnBnClickedClassMedic)
	ON_BN_CLICKED(IDC_CLASS_Scout, OnBnClickedClassScout)
	ON_BN_CLICKED(IDC_CLASS_Sniper, OnBnClickedClassSniper)
	ON_BN_CLICKED(IDC_CLASS_Soldier, OnBnClickedClassSoldier)
	ON_BN_CLICKED(IDC_CLASS_Enemy, OnBnClickedClassEnemy)
	ON_BN_CLICKED(IDC_WEAPON_PLAZMAGUN, OnBnClickedWeaponPlazmagun)
	ON_BN_CLICKED(IDC_WEAPON_PK_PlazmaGun, OnBnClickedWeaponPkPlazmagun)
END_MESSAGE_MAP()


// CAnimFlagsDlg message handlers

void CAnimFlagsDlg::OnBnClickedPoseStand()
{
	UpdateItem( IDC_POSE_STAND );
}

void CAnimFlagsDlg::OnBnClickedPoseCrouch()
{
	UpdateItem( IDC_POSE_CROUCH );
}

void CAnimFlagsDlg::OnBnClickedPoseCrawl()
{
	UpdateItem( IDC_POSE_CRAWL );
}

void CAnimFlagsDlg::OnBnClickedWeaponNoweapon()
{
	UpdateItem( IDC_WEAPON_NoWeapon );
}

void CAnimFlagsDlg::OnBnClickedWeaponItem()
{
	UpdateItem( IDC_WEAPON_Item );
}

void CAnimFlagsDlg::OnBnClickedWeaponPistol()
{
	UpdateItem( IDC_WEAPON_Pistol );
}

void CAnimFlagsDlg::OnBnClickedWeaponRifle()
{
	UpdateItem( IDC_WEAPON_Rifle );
}

void CAnimFlagsDlg::OnBnClickedWeaponSubmachinegun()
{
	UpdateItem( IDC_WEAPON_SubMachineGun );
}

void CAnimFlagsDlg::OnBnClickedWeaponMachinegun()
{
	UpdateItem( IDC_WEAPON_MachineGun );
}

void CAnimFlagsDlg::OnBnClickedWeaponRlauncher()
{
	UpdateItem( IDC_WEAPON_RLauncher );
}

void CAnimFlagsDlg::OnBnClickedWeaponKnife()
{
	UpdateItem( IDC_WEAPON_RLauncher );
}

void CAnimFlagsDlg::OnBnClickedWeaponMachete()
{
	UpdateItem( IDC_WEAPON_Machete );
}

void CAnimFlagsDlg::OnBnClickedWeaponKatana()
{
	UpdateItem( IDC_WEAPON_Katana );
}

void CAnimFlagsDlg::OnBnClickedWeaponMdetector()
{
	UpdateItem( IDC_WEAPON_MDetector );
}

void CAnimFlagsDlg::OnBnClickedWeaponPkshooter()
{
	UpdateItem( IDC_WEAPON_PKShooter );
}

void CAnimFlagsDlg::OnBnClickedWeaponPkslasher()
{
	UpdateItem( IDC_WEAPON_PKSlasher );
}

void CAnimFlagsDlg::OnBnClickedWeaponPkrepairer()
{
	UpdateItem( IDC_CLASS_Enemy );
}

void CAnimFlagsDlg::OnBnClickedMale()
{
	UpdateItem( IDC_MALE );
}

void CAnimFlagsDlg::OnBnClickedFemale()
{
	UpdateItem( IDC_FEMALE );
}

void CAnimFlagsDlg::OnBnClickedCombat()
{
	UpdateItem( IDC_COMBAT );
}

void CAnimFlagsDlg::OnBnClickedRealtime()
{
	UpdateItem( IDC_REALTIME );
}

void CAnimFlagsDlg::OnBnClickedClassEngineer()
{
	UpdateItem( IDC_CLASS_Engineer );
}

void CAnimFlagsDlg::OnBnClickedClassGrenadier()
{
	UpdateItem( IDC_CLASS_Grenadier );
}

void CAnimFlagsDlg::OnBnClickedClassMedic()
{
	UpdateItem( IDC_CLASS_Medic );
}

void CAnimFlagsDlg::OnBnClickedClassScout()
{
	UpdateItem( IDC_CLASS_Scout );
}

void CAnimFlagsDlg::OnBnClickedClassSniper()
{
	UpdateItem( IDC_CLASS_Sniper );
}

void CAnimFlagsDlg::OnBnClickedClassSoldier()
{
	UpdateItem( IDC_CLASS_Soldier );
}

void CAnimFlagsDlg::OnBnClickedClassEnemy()
{
	UpdateItem( IDC_CLASS_Enemy );
}

void CAnimFlagsDlg::OnBnClickedWeaponPlazmagun()
{
	UpdateItem( IDC_WEAPON_PLAZMAGUN );
}

void CAnimFlagsDlg::OnBnClickedWeaponPkPlazmagun()
{
	UpdateItem( IDC_WEAPON_PK_PlazmaGun );
}
