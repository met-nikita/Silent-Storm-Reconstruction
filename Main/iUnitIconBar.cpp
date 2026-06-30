#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "wInterface.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataRPG.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iLogPanel.h"
#include "iTopPanel.h"
#include "iUnitPanel.h"
#include "iInventoryPanel.h"
#include "iCharacterPanel.h"
#include "iActionDecorator.h"
#include "UIWrap.h"
#include "iUnitIconBar.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int
	N_ICON_UNDEFAINED		= -1,
	N_ICON_UNAVAILABLE	= -2;
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUnitsGroupPose( NGame::IMission *pMission )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	int nPose = N_ICON_UNAVAILABLE;
	bool bPoseSet = false;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		NAI::EPose ePose = unitsSet[nTemp]->GetUnit()->GetPosition().GetPose();
		if ( ePose == NAI::RUN && unitsSet[nTemp]->GetUnit()->IsCarryingCorpse() )
			ePose = NAI::WALK;
		if ( !bPoseSet )
		{
			nPose = ePose;
			bPoseSet = true;
		}
		else if ( nPose != ePose )
			nPose = N_ICON_UNDEFAINED;
	}

	return nPose;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUnitsGroupStrafe( NGame::IMission *pMission )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	int nStrafe = N_ICON_UNAVAILABLE;
	bool bPoseSet = false;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		int nNewStrafe = 0;
		if ( unitsSet[nTemp]->GetUnit()->IsStrafing() )
			nNewStrafe = 1;

		if ( !bPoseSet )
		{
			nStrafe = nNewStrafe;
			bPoseSet = true;
		}
		else if ( nStrafe != nNewStrafe )
			nStrafe = N_ICON_UNDEFAINED;
	}

	return nStrafe;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUnitsGroupHide( NGame::IMission *pMission )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	int nHide = N_ICON_UNAVAILABLE;
	bool bSet = false;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		int nNewHide = 0;
		if ( unitsSet[nTemp]->GetUnit()->IsHiding() )
			nNewHide = 1;

		if ( !bSet )
		{
			nHide = nNewHide;
			bSet = true;
		}
		else if ( nHide != nNewHide )
			nHide = N_ICON_UNDEFAINED;
	}

	return nHide;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUnitsGroupWeaponMode( NGame::IMission *pMission )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	int nMode = N_ICON_UNAVAILABLE;
	bool bModeSet = false;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		CPtr<NRPG::IInventoryItem> pItem = unitsSet[nTemp]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive();
		CPtr<NRPG::IWeaponItemInfo> pWeapon = dynamic_cast<NRPG::IWeaponItemInfo*>( pItem.GetPtr() );
		if ( IsValid( pWeapon ) )
		{
			NDb::EShootMode eMode = pWeapon->GetShootMode();
			if ( !bModeSet )
			{
				nMode = eMode;
				bModeSet = true;
			}
			else if ( nMode != eMode )
				nMode = N_ICON_UNDEFAINED;
		}
		else
		{
			nMode = N_ICON_UNAVAILABLE;
		}
	}

	return nMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUnitsGroupGrenadeMode( NGame::IMission *pMission )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	int nMode = N_ICON_UNAVAILABLE;
	bool bModeSet = false;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		CPtr<NRPG::IInventoryItem> pItem = unitsSet[nTemp]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive();
		CPtr<NRPG::IGrenadeItemInfo> pGrenade = dynamic_cast<NRPG::IGrenadeItemInfo*>( pItem.GetPtr() );
		if ( IsValid( pGrenade ) )
		{
			NRPG::EGrenadeMode eMode = pGrenade->GetMode();
			if ( !bModeSet )
			{
				nMode = eMode;
				bModeSet = true;
			}
			else if ( nMode != eMode )
				nMode = N_ICON_UNDEFAINED;
		}
		else
		{
			nMode = N_ICON_UNAVAILABLE;
		}
	}

	return nMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIconBarSet: public CObjectBase
{
private:
	struct SIconButton
	{
		ZDATA
		int nPriority;
		CPtr<CComplexButton> pButton;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPriority); f.Add(3,&pButton); return 0; }
	};
	ZDATA
	CPtr<NGame::IMission> pMission;
	////
	vector<SIconButton> iconsSet;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMission); f.Add(3,&iconsSet); return 0; }

protected:
	enum { N_ANY_VALUE = -1 };
	enum ESlot
	{
		SLOT_R1C1	= 0,
		SLOT_R1C2	= 1,
		SLOT_R1C3	= 2,
		SLOT_R1C4	= 3,
		SLOT_R2C1	= 4,
		SLOT_R2C2	= 5,
		SLOT_R2C3	= 6,
		SLOT_R2C4	= 7
	};

public:
	CIconBarSet() {}
	CIconBarSet( NGame::IMission *_pGame, const vector<CPtr<CComplexButton> > &_iconsSet );

	NGame::IMission* GetMission() const;

	void CreateButton( int nNormalIcon, int nDisabledIcon, CComplexButton::EState eState, int nToolTipID, int nSlot, int nPriority, int nUnitAction, int nUnitState, const string &szID );

	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CIconBarSet::CIconBarSet( NGame::IMission *_pGame, const vector<CPtr<CComplexButton> > &_iconsSet ):
	pMission( _pGame )
{
	iconsSet.resize( _iconsSet.size() );
	for ( int nTemp = 0; nTemp < iconsSet.size(); nTemp++ )
		iconsSet[nTemp].pButton = _iconsSet[nTemp];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::IMission* CIconBarSet::GetMission() const
{
	return pMission;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CIconBarSet::CreateButton( int nNormalIcon, int nDisabledIcon, CComplexButton::EState eState, int nToolTipID, int nSlot, int nPriority, int nUnitAction, int nUnitState, const string &szID )
{
	int nState = pMission->GetUnitsWorldState();
	if ( ( nUnitState != N_ANY_VALUE ) && ( nUnitState != nState ) )
		return;
	if ( iconsSet[nSlot].nPriority < nPriority )
		return;

	NGame::SActionInfo sActionInfo;
	pMission->GetActionInfo( NGame::EUnitAction( nUnitAction ), &sActionInfo );
	if ( !sActionInfo.bAvailable )
		return;

	CPtr<NDb::CUITexture> pNormalIcon, pCheckedIcon, pDisabledIcon;
	if ( nNormalIcon != -1 )
		pNormalIcon = NDb::GetUITexture( nNormalIcon );
	if ( nDisabledIcon != -1 )
		pDisabledIcon = NDb::GetUITexture( nDisabledIcon );

	iconsSet[nSlot].nPriority = nPriority;

	CPtr<CComplexButton> pButton = iconsSet[nSlot].pButton;
	pButton->Set( pNormalIcon, pDisabledIcon, eState, szID );
	pButton->SetStyle( STYLE_VISIBLE, true );
	pButton->SetStyle( STYLE_ENABLED, sActionInfo.bOk );

	CPtr<CToolTip> pToolTip = pButton->GetToolTip();
	pToolTip->SetText( GetDBString( nToolTipID ) );

	if ( sActionInfo.bOk )
		pToolTip->SetVal( L"ap", sActionInfo.nActionAP );
	else
		pToolTip->SetVal( L"ap", L"N/A" );

	if ( sActionInfo.bEnoughAP )
		pButton->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );
	else
		pButton->SetColor( NGfx::SPixel8888( 0x5F, 0x5F, 0xBF, 0xFF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CIconBarSet::Update()
{
	for( int nTemp = 0; nTemp < iconsSet.size(); nTemp++ )
	{
		iconsSet[nTemp].nPriority = 0xFF;
		iconsSet[nTemp].pButton->SetStyle( STYLE_VISIBLE, false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMainIconBarSet: public CIconBarSet
{
	OBJECT_BASIC_METHODS(CMainIconBarSet)
private:
	ZDATA_(CIconBarSet)
	CObj<CIconBarSet> pPoseIconBar;
	CObj<CIconBarSet> pWeaponModeIconBar;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CIconBarSet*)this); f.Add(2,&pPoseIconBar); f.Add(3,&pWeaponModeIconBar); return 0; }

public:
	CMainIconBarSet() {}
	CMainIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet );

	virtual void Update();
};                                                                                                  
////////////////////////////////////////////////////////////////////////////////////////////////////
CMainIconBarSet::CMainIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet ):
	CIconBarSet( pMission, iconsSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainIconBarSet::Update()
{
	CIconBarSet::Update();

	int nState = GetMission()->GetUnitsState();
	NGame::SActionInfo sActionInfo;
	switch( nState )
	{
	case NGame::N_UNITSTATE_DEFAULT:
		{
			// R1C1
			CreateButton( 386, 420, CComplexButton::NORMAL, 4277, SLOT_R1C1, 0, NGame::UA_ATTACK, NWorld::CUnit::ST_NORMAL_MELEE, "attack" );
			CreateButton( 385, 425, CComplexButton::NORMAL, 4278, SLOT_R1C1, 0, NGame::UA_ATTACK, NWorld::CUnit::ST_NORMAL_KNIFE, "attack" );
			CreateButton( 384, 424, CComplexButton::NORMAL, 4279, SLOT_R1C1, 0, NGame::UA_ATTACK, NWorld::CUnit::ST_NORMAL_GRENADE,	"attack" );
			CreateButton( 648, 649, CComplexButton::NORMAL, 4279, SLOT_R1C1, 0, NGame::UA_MINE, NWorld::CUnit::ST_NORMAL_MINE,	"setmine" );
			CreateButton( 387, 419, CComplexButton::NORMAL, 4280, SLOT_R1C1, 1, NGame::UA_ATTACK, N_ANY_VALUE, "attack" );
			CreateButton( 388, 426, CComplexButton::NORMAL, 4291, SLOT_R1C1, 1, NGame::UA_HEAL, NWorld::CUnit::ST_NORMAL_MEDKIT, "firstaid" );
			CreateButton( 443, 475, CComplexButton::NORMAL, 4290, SLOT_R1C1, 1, NGame::UA_DROPCORPSE, NWorld::CUnit::ST_CARRY_CORPSE, "dropcorpse" );
			// R1C2
			CreateButton( 474, 510, CComplexButton::NORMAL, 4466, SLOT_R1C2, 0, NGame::UA_CONTINUE, N_ANY_VALUE, "continue" );
			CreateButton( 390, 421, CComplexButton::NORMAL, 4281, SLOT_R1C2, 1, NGame::UA_MOVE, N_ANY_VALUE, "move" );
			// R1C3, R1C4
			CreateButton( 389, 427, CComplexButton::NORMAL, 4282, SLOT_R1C3, 0, NGame::UA_LOOK, N_ANY_VALUE, "unit_rotate" );
			CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R1C4, 0, NGame::UA_STOP, N_ANY_VALUE, "cancelaction" );
			// R2C1
			GetMission()->GetActionInfo( NGame::UA_ATTACK, &sActionInfo );
			if ( sActionInfo.bAvailable )
			{
				int nWeaponMode = GetUnitsGroupWeaponMode( GetMission() );
				int nGrenadeMode = GetUnitsGroupGrenadeMode( GetMission() );
				if ( nWeaponMode != N_ICON_UNAVAILABLE )
				{
					switch( nWeaponMode )
					{
					case N_ICON_UNDEFAINED:
						CreateButton( 516, 572, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_Snap:
						CreateButton( 501, 576, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_Aimed:
						CreateButton( 503, 578, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_Careful:
						CreateButton( 502, 577, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_ShortBurst:
						CreateButton( 500, 575, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_LongBurst:
						CreateButton( 499, 574, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					case NDb::SM_Snipe:
						CreateButton( 498, 573, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_weaponmode" );
						break;
					}
				}
				else if ( nGrenadeMode != N_ICON_UNAVAILABLE )
				{
					switch( nGrenadeMode )
					{
					case N_ICON_UNDEFAINED:
						CreateButton( 652, 653, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_grenademode" );
						break;
					case NRPG::GM_THROW:
						CreateButton( 652, 653, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_grenademode" );
						break;
					case NRPG::GM_SETTRAP:
						CreateButton( 650, 651, CComplexButton::NORMAL, 4522, SLOT_R2C1, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_grenademode" );
						break;
					}
				}
			}
			// R2C2
			GetMission()->GetActionInfo( NGame::UA_MOVE, &sActionInfo );
			if ( sActionInfo.bAvailable )
			{
				int nPose = GetUnitsGroupPose( GetMission() );
				switch( nPose )
				{
				case N_ICON_UNDEFAINED:
					CreateButton( 504, 571, CComplexButton::NORMAL, 4523, SLOT_R2C2, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_poseselect" );
					break;
				case NAI::RUN:
					CreateButton( 393, 440, CComplexButton::NORMAL, 4523, SLOT_R2C2, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_poseselect" );
					break;
				case NAI::WALK:
					CreateButton( 394, 441, CComplexButton::NORMAL, 4523, SLOT_R2C2, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_poseselect" );
					break;
				case NAI::CROUCH:
					CreateButton( 392, 439, CComplexButton::NORMAL, 4523, SLOT_R2C2, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_poseselect" );
					break;
				case NAI::CRAWL:
					CreateButton( 391, 438, CComplexButton::NORMAL, 4523, SLOT_R2C2, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "submenu_poseselect" );
					break;
				}
			}
			// R2C3
			int nStrafe = GetUnitsGroupStrafe( GetMission() );
			switch( nStrafe )
			{
			case 0:
				CreateButton( 505, 509, CComplexButton::UNCHECKED, 4524, SLOT_R2C3, 1, NGame::UA_STRAFE, N_ANY_VALUE, "pose_strafe" );
				break;
			case 1:
				CreateButton( 505, 509, CComplexButton::CHECKED, 4524, SLOT_R2C3, 1, NGame::UA_STRAFE, N_ANY_VALUE, "pose_strafe" );
				break;
			}
			CreateButton( 382, 421, CComplexButton::NORMAL, 4283, SLOT_R2C3, 0, NGame::UA_EXITPK, N_ANY_VALUE, "exitpk" );
			// R2C4
			int nHide = GetUnitsGroupHide( GetMission() );
			switch( nHide )
			{
			case 0:
				CreateButton( 506, 508, CComplexButton::UNCHECKED, 4525, SLOT_R2C4, 0, NGame::UA_HIDE, N_ANY_VALUE, "hide" );
				break;
			case 1:
				CreateButton( 506, 508, CComplexButton::CHECKED, 4525, SLOT_R2C4, 0, NGame::UA_HIDE, N_ANY_VALUE, "hide" );
				break;
			}
			break;
		}
	case NGame::N_UNITSTATE_CANNON:
		{
			CreateButton( 387, 419, CComplexButton::NORMAL, 4289, SLOT_R1C1, 0, NGame::UA_ATTACK, NWorld::CUnit::ST_MACHINE_GUN, "attack" );
			CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_STOP, N_ANY_VALUE, "cancelaction" );
			break;
		}
	case NGame::N_UNITSTATE_SNIPE:
		{
			CreateButton( 490, 511, CComplexButton::NORMAL, 4441, SLOT_R1C1, 0, NGame::UA_COLLECTAP_ALL, NWorld::CUnit::ST_SNIPE, "collectap_all" );
			CreateButton( 489, 513, CComplexButton::NORMAL, 4442, SLOT_R1C2, 0, NGame::UA_COLLECTAP_MAX, NWorld::CUnit::ST_SNIPE, "collectap_max" );
			CreateButton( 488, 514, CComplexButton::NORMAL, 4443, SLOT_R1C3, 0, NGame::UA_COLLECTAP_10AP, NWorld::CUnit::ST_SNIPE, "collectap_10ap" );
			CreateButton( 487, 515, CComplexButton::NORMAL, 4444, SLOT_R1C4, 0, NGame::UA_COLLECTAP_1AP, NWorld::CUnit::ST_SNIPE, "collectap_1ap" );
			CreateButton( 491, 512, CComplexButton::NORMAL, 4445, SLOT_R2C1, 1, NGame::UA_SNIPE_ATTACK, NWorld::CUnit::ST_SNIPE, "snipe_attack" );
			CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_STOP, N_ANY_VALUE, "cancelaction" );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPoseIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPoseIconBarSet: public CIconBarSet
{
	OBJECT_BASIC_METHODS(CPoseIconBarSet)
private:
	ZDATA_(CIconBarSet)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CIconBarSet*)this); return 0; }

public:
	CPoseIconBarSet() {}
	CPoseIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet );

	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPoseIconBarSet::CPoseIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet ):
	CIconBarSet( pMission, iconsSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPoseIconBarSet::Update()
{
	CIconBarSet::Update();

	int nPose = GetUnitsGroupPose( GetMission() );

	CreateButton( 393, 440, ( nPose == NAI::RUN ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, 4284, SLOT_R1C1, 0, NGame::UA_POSERUN, N_ANY_VALUE, "pose_run" );
	CreateButton( 394, 441, ( nPose == NAI::WALK ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, 4285, SLOT_R1C2, 0, NGame::UA_POSEWALK, N_ANY_VALUE, "pose_normal" );
	CreateButton( 392, 439, ( nPose == NAI::CROUCH ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, 4286, SLOT_R1C3, 0, NGame::UA_POSECROUCH, N_ANY_VALUE, "pose_crouch" );
	CreateButton( 391, 438, ( nPose == NAI::CRAWL ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, 4288, SLOT_R1C4, 0, NGame::UA_POSECRAWL, N_ANY_VALUE, "pose_crawl" );
	////
	CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_STOP, N_ANY_VALUE, "~dummy~" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWeaponModeIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWeaponModeIconBarSet: public CIconBarSet
{
	OBJECT_BASIC_METHODS(CWeaponModeIconBarSet)
private:
	ZDATA_(CIconBarSet)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CIconBarSet*)this); return 0; }

public:
	CWeaponModeIconBarSet() {}
	CWeaponModeIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet );

	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CWeaponModeIconBarSet::CWeaponModeIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet ):
	CIconBarSet( pMission, iconsSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWeaponModeIconBarSet::Update()
{
	CIconBarSet::Update();

	struct SWeaponModeInfo
	{
		NDb::EShootMode eMode;

		CHAR* pszID;
		int nIcon;
		int nDisabledIcon;
		int nToolTipID;
	};
	static SWeaponModeInfo pWeaponModes[] = 
	{
		{ NDb::SM_Snap, "weapon_snapshot", 501, 576, 4298 },
		{ NDb::SM_Aimed, "weapon_aimedshot", 503, 578, 4299 },
		{ NDb::SM_Careful, "weapon_carefulshot", 502, 577, 4300 },
		{ NDb::SM_ShortBurst, "weapon_shortburst", 500, 575, 4301 },
		{ NDb::SM_LongBurst, "weapon_longburst", 499, 574, 4302 },
		{ NDb::SM_Snipe, "weapon_snipeshot", 498, 573, 4303 },
	};

	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
	{
		GetMission()->SetActionIconsSet( NGame::AIS_MAIN );
		return;
	}

	int nWeaponMode = GetUnitsGroupWeaponMode( GetMission() );

	CPtr<NRPG::IInventoryInfo> pInventory = unitsSet[0]->GetUnit()->GetRPG()->GetInventoryInfo();
	CPtr<NRPG::IInventoryItem> pItem = pInventory->GetActive();
	CPtr<NRPG::IWeaponItemInfo> pWeapon = dynamic_cast<NRPG::IWeaponItemInfo*>( pItem.GetPtr() );
	if ( !IsValid( pWeapon ) )
	{
		GetMission()->SetActionIconsSet( NGame::AIS_MAIN );
		return;
	}

	int nCount = 0;
	NDb::EShootMode eMode = pWeapon->GetShootMode();
	CPtr<NDb::CRPGWeapon> pRPGWeapon = pWeapon->GetDBWeapon();
	for ( int nTemp = 0; nTemp < ARRAY_SIZE( pWeaponModes ); nTemp++ )
	{
		if ( nCount >= 4 )
			break;
		const SWeaponModeInfo &sModeInfo = pWeaponModes[nTemp];
		if ( !pRPGWeapon->shootModes[sModeInfo.eMode] )
			continue;

		CreateButton( sModeInfo.nIcon, sModeInfo.nDisabledIcon, ( sModeInfo.eMode == nWeaponMode ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, sModeInfo.nToolTipID, SLOT_R1C1 + nCount, 0, NGame::UA_DEFAULT, N_ANY_VALUE, sModeInfo.pszID );
		nCount++;
	}
	////
	CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "~dummy~" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrenadeModeIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrenadeModeIconBarSet: public CIconBarSet
{
	OBJECT_BASIC_METHODS(CGrenadeModeIconBarSet)
private:
	ZDATA_(CIconBarSet)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CIconBarSet*)this); return 0; }

public:
	CGrenadeModeIconBarSet() {}
	CGrenadeModeIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet );

	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrenadeModeIconBarSet::CGrenadeModeIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet ):
	CIconBarSet( pMission, iconsSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrenadeModeIconBarSet::Update()
{
	CIconBarSet::Update();

	struct SGrenadeModeInfo
	{
		int nMode;

		CHAR* pszID;
		int nIcon;
		int nDisabledIcon;
		int nToolTipID;
	};
	static SGrenadeModeInfo pGrenadeModes[] = 
	{
		{ NRPG::GM_THROW, "grenade_throw", 652, 653, 4298 },
		{ NRPG::GM_SETTRAP, "grenade_settrap", 650, 651, 4299 },
	};

	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
	{
		GetMission()->SetActionIconsSet( NGame::AIS_MAIN );
		return;
	}

	int nGrenadeMode = GetUnitsGroupGrenadeMode( GetMission() );
	if ( nGrenadeMode == N_ICON_UNAVAILABLE )
	{
		GetMission()->SetActionIconsSet( NGame::AIS_MAIN );
		return;
	}

	int nCount = 0;
	for ( int nTemp = 0; nTemp < ARRAY_SIZE( pGrenadeModes ); nTemp++ )
	{
		if ( nCount >= 4 )
			break;

		const SGrenadeModeInfo &sModeInfo = pGrenadeModes[nTemp];
		CreateButton( sModeInfo.nIcon, sModeInfo.nDisabledIcon, ( sModeInfo.nMode == nGrenadeMode ) ? CComplexButton::CHECKED : CComplexButton::UNCHECKED, sModeInfo.nToolTipID, SLOT_R1C1 + nCount, 0, NGame::UA_DEFAULT, N_ANY_VALUE, sModeInfo.pszID );
		nCount++;
	}
	////
	CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_DEFAULT, N_ANY_VALUE, "~dummy~" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CForcedIconBarSet
////////////////////////////////////////////////////////////////////////////////////////////////////
class CForcedIconBarSet: public CIconBarSet
{
	OBJECT_BASIC_METHODS(CForcedIconBarSet)
private:
	ZDATA_(CIconBarSet)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CIconBarSet*)this); return 0; }

public:
	CForcedIconBarSet() {}
	CForcedIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet );

	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CForcedIconBarSet::CForcedIconBarSet( NGame::IMission *pMission, const vector<CPtr<CComplexButton> > &iconsSet ):
	CIconBarSet( pMission, iconsSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CForcedIconBarSet::Update()
{
	CIconBarSet::Update();

	CreateButton( 382, 570, CComplexButton::NORMAL, 4283, SLOT_R2C4, 0, NGame::UA_STOP, N_ANY_VALUE, "cancelaction" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitIconsBar
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitIconsBar::CUnitIconsBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), iconsSet( 8 ), eLastSet( NGame::EActionIconsSet( -1 ) )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitIconsBar::SetIconBar( CIconBarSet *pIcons )
{
	pIconBar = pIcons;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitIconsBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			pMission->SetActionIconsSet( NGame::AIS_MAIN );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			for ( int nTemp = 0; nTemp < iconsSet.size(); nTemp++ )
				iconsSet[nTemp] = new CComplexButton( sEvent.pLoader->GetControl( NStr::Format( "icon_%d", ( nTemp + 1 ) ) ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pIconBar = new CMainIconBarSet( pMission, iconsSet );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitIconsBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( pMission->GetState()->GetType() == NGame::IState::FORCED )
		pMission->SetActionIconsSet( NGame::AIS_FORCED );
	else if ( pMission->GetActionIconsSet() == NGame::AIS_FORCED )
		pMission->SetActionIconsSet( NGame::AIS_MAIN );

	NGame::EActionIconsSet eIconsSet = pMission->GetActionIconsSet();
	if ( eLastSet != eIconsSet )
	{
		eLastSet = eIconsSet;

		switch( eIconsSet )
		{
		case NGame::AIS_MAIN:
			{
				pIconBar = new CMainIconBarSet( pMission, iconsSet );
				break;
			}
		case NGame::AIS_POSES:
			{
				pIconBar = new CPoseIconBarSet( pMission, iconsSet );
				break;
			}
		case NGame::AIS_WEAPONMODES:
			{
				pIconBar = new CWeaponModeIconBarSet( pMission, iconsSet );
				break;
			}
		case NGame::AIS_GRENADEMODES:
			{
				pIconBar = new CGrenadeModeIconBarSet( pMission, iconsSet );
				break;
			}
		case NGame::AIS_FORCED:
			{
				pIconBar = new CForcedIconBarSet( pMission, iconsSet );
				break;
			}
		}
	}

	if ( IsValid( pIconBar ) )
		pIconBar->Update();

	return CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
BASIC_REGISTER_CLASS(CIconBarSet)
REGISTER_SAVELOAD_CLASS( 0xB0830171, CMainIconBarSet );
REGISTER_SAVELOAD_CLASS( 0xB0830172, CPoseIconBarSet );
REGISTER_SAVELOAD_CLASS( 0xB0830173, CWeaponModeIconBarSet );
REGISTER_SAVELOAD_CLASS( 0xB0830174, CForcedIconBarSet );
REGISTER_SAVELOAD_CLASS( 0xB0241945, CUnitIconsBar );
REGISTER_SAVELOAD_CLASS( 0xB0241946, CGrenadeModeIconBarSet );
