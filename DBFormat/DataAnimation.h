#ifndef __DATAANIMATION_H_
#define __DATAANIMATION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkeleton;
class CSound;
class CSide;
class CAnimation: public CDBRecord
{
	OBJECT_BASIC_METHODS(CAnimation);
public:
	enum EPoseWeaponFlags
	{
		POSE_STAND = 0x0001,
		POSE_CROUCH = 0x0002,
		POSE_CRAWL = 0x0004,

		WEAPON_NONE = 0x0100,
		WEAPON_ITEM = 0x0200,
		WEAPON_PISTOL = 0x0400,
		WEAPON_RIFLE = 0x0800,
		WEAPON_SUB_MACHINE_GUN = 0x1000,
		WEAPON_KNIFE = 0x2000,
		WEAPON_MACHINE_GUN = 0x4000,
		WEAPON_RLAUNCHER = 0x8000,
		WEAPON_MINE_DETECTOR = 0x10000,
		PK_WEAPON_SHOOTER = 0x20000,
		PK_WEAPON_SLASHER = 0x40000,
		PK_WEAPON_REPAIRER = 0x80000,
		WEAPON_KATANA = 0x100000,
		WEAPON_PLAZMAGUN = 0x200000,
		PK_WEAPON_PLAZMAGUN = 0x400000,
	};
	enum EClassSexFlags
	{
		SEX_MALE = 0x0001,
		SEX_FEMALE = 0x0002,
		IN_COMBAT = 0x0004,
		IN_REALTIME = 0x0008,
		SCOUT = 0x0010,
		SNIPER = 0x0020,
		GRENADIER = 0x0040,
		SOLDIER = 0x0080,
		MEDIC = 0x0100,
		ENGINEER = 0x0200,
		ENEMY = 0x0400,
	};
	enum EType
	{
		POSE = 1,
		START_MOVE,
		MOVE,
		START_RUN,
		RUN,
		START_ATTACK,
		ATTACK,
		ATTACK_UP,
		ATTACK_DOWN,
		TURN_LEFT,
		TURN_RIGHT,
		POSE_STRAFE,
		START_STRAFE_F,
		STRAFE_F,
		START_STRAFE_L,
		STRAFE_L,
		START_STRAFE_R,
		STRAFE_R,
		START_STRAFE_B,
		STRAFE_B,
		CLIMB_LOW,
		CLIMB_HIGH,
		JUMP_LOW,
		JUMP_HIGH,
		JUMP_START,
		CLIMB_FINISH,
		ACTIVATE,
		DEACTIVATE,
		POSE_ITEM,
		USE,
		CHANGE_POSE,
		DEATH,
		POSE_CORPSE,
		TAKE_CORPSE,
		DROP_CORPSE,
		START_MOVE_CORPSE,
		MOVE_CORPSE,
		ATTACK_LD,
		ATTACK_RD,
		ATTACK_LU,
		ATTACK_RU,
		OPEN,
		RELOAD,
		IDLE,
		POSE_HEAL,
		START_HEAL,
		HEAL,
		ATTACK_CEILING,
		ATTACK_FLOOR,
		ENTER_LADDER_UP,
		ENTER_LADDER_DOWN,
		LEAVE_LADDER_UP,
		LEAVE_LADDER_DOWN,
		MOVE_LADDER_UP,
		MOVE_LADDER_DOWN,
		JUMP_LADDER,
		PUT_BACKPACK,
		GET_BACKPACK,
		THROW_KNIFE,
		FALL,
		DESTRUCT_1,
		DESTRUCT_2,
		DESTRUCT_3,
		DESTRUCT_4,
	};
	CPtr<CSkeleton> pSkeleton;
	EType nType;
	int nPoseWeaponFlags;
	int nClassSexFlags;
	float fSpeed;
	string szParams;
	int nTime1;
	int nStepTime1;
	int nStepTime2;
	float fRndWeight;
	int nAngle;
	bool bReverse;
	float fFallHeight;

	CPtr<CSide> pSide;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAnimationVector
{
	vector< CPtr<CAnimation> > anims;
	//
	CAnimation* GetAnimation( int nFlags, 
		const char *pszParams = 0, int nClassSexFlags = 0, CSide *pSide = 0, bool bMostPossible = false ) const;
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map< int, SAnimationVector > CAnimationMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGeometry;
class CSkeleton: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSkeleton);
public:
	CAnimationMap pAnimations;
	CPtr<CAIGeometry> pAIMeshLie, pAIMeshCrouch, pAIMeshStay;

	CAnimation* GetAnimation( const CAnimation::EType nType, const int nFlags = 0, 
		const char *pszParams = 0, int nClassSexFlags = 0, CSide *pSide = 0, bool bMostPossible = false ) const;
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimation *GetDBAnimation( int nID );
//
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAANIMATION_H_
