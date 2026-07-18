#ifndef __DATARPG_H_
#define __DATARPG_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DataConst.h"
namespace NAI
{
	enum EHitLocation;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CDBDifficulty;   // fwd for SItemAssign::pDifficulty (release save-format; CPtr needs only fwd-decl)
class CMedal;          // fwd for CSide::medals (DataMisc.h; retail CSide save tag 11)
class CSide;
class CNationality;
class CModel;
class CSound;
class CAISound;
class CTSound;
class CTRndModel;
class CString;
class CUITexture;
class CTEffect;
class CComplexHead;
class CUITexture;
class CUIContainer;
class CPlacableObject;
class CDBDialogPers;
class CDBPerk;
class CTRPGChest;
//
enum EAmmoColor
{
	EAC_NORMAL = 0,
	EAC_BLUE,
	EAC_RED,
	EAC_YELLOW
};

const int N_DEFAULT_ARMOR = 4;
const int N_HUMAN_BODY_ARMOR = 1;
////////////////////////////////////////////////////////////////////////////////////////////////////
// SKILLS common set of RPG skills 
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESkillType
{
	// weapons skills
	ST_MELEE = 0,
	ST_SHOOTING,
	ST_THROWING,
	ST_BURST,
	ST_SNIPE,
	// non-combat skills
	ST_STEALTH,
	ST_SPOT,
	ST_MEDICINE,
	ST_ENGINEERING,
	// passive skills
	ST_VP,
	ST_AP,
	ST_IC,
	ST_INTERRUPT,
	ST_LEVEL,
	// stats
	ST_STR,
	ST_DEX,
	ST_INT,

	SKILL_TYPE_NUMBERS
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRPGSkills
{
	int skills[SKILL_TYPE_NUMBERS];

	int &operator [] ( int n ) { return skills[n]; }
	SRPGSkills() { Clear(); }
	void Clear() { Zero(*this); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFirstAidEffect
{
	FAE_NORMAL,
	FAE_CRITICAL_ONLY,
	FAE_TEMP_REMOVE_PENALTIES,
	FAE_BOOST_VP,
	FAE_TEMP_STOP_BLEEDING,
	FAE_REMOVE_BLEEDING,
	FAE_REPAIR_PK   // retail =6: panzerklein repair kit -- gates on ST_ENGINEERING, not ST_MEDICINE (CSlot::Draw @0x1c34d0, healer @0x3ca340)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// RPG data tables
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGDmgToArmor: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGDmgToArmor);
public:
	int armors[12];
	int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&armors); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGMaterial: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGMaterial);
public:
	// materials
	enum
	{
		HUMAN_BODY = 1,	// �������
		GROUND,			// �����
		STONE,			// ������
		WOOD,			// ������
		BRICK,			// ������
		CLOTH = 20,		// �����
		STEEL,			// ����� (������ ����������)
		ARMOR_STEEL1,	// ����������� ����������
		ARMOR_STEEL2,	// ���������� ���������������
	};
	ZDATA_(CDBRecord)
	int nThreshold, nVP, nDR;
	float fDensity;
	float fTransparency;
	float fUltimateMoment;
	float fUltimatePresure;
	float fWeight; // phisical density
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nThreshold); f.Add(3,&nVP); f.Add(4,&nDR); f.Add(5,&fDensity); f.Add(6,&fTransparency); f.Add(7,&fUltimateMoment); f.Add(8,&fUltimatePresure); f.Add(9,&fWeight); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTMaterial;
class CRPGArmor: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGArmor);
public:
	ZDATA_(CDBRecord)
	CPtr<CTSound> pSoundStep;
	CPtr<CTSound> pSoundShot;
	CPtr<CTEffect> pShotEffect;
	int nAISoundType;
	CPtr<CRPGMaterial> pMaterial;
	int nGrenadeSoundType;
	int nGrenadeExplostionType;
	CPtr<CTMaterial> pShotMaterial;
	float fShotRadius;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pSoundStep); f.Add(3,&pSoundShot); f.Add(4,&pShotEffect); f.Add(5,&nAISoundType); f.Add(6,&pMaterial); f.Add(7,&nGrenadeSoundType); f.Add(8,&nGrenadeExplostionType); f.Add(9,&pShotMaterial); f.Add(10,&fShotRadius); f.Add(11,&bBreakableByGrenade); f.Add(12,&fRicochetMaxCos); f.Add(13,&fRicochetProbability); f.Add(14,&pShotEffect1); f.Add(15,&pShotEffect2); f.Add(16,&pShotEffect3); return 0; }
	bool bBreakableByGrenade = false;
	float fRicochetMaxCos = 0.0f;
	float fRicochetProbability = 0.0f;
	CPtr<CTEffect> pShotEffect1;
	CPtr<CTEffect> pShotEffect2;
	CPtr<CTEffect> pShotEffect3;

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGUniform;
struct SUniformItem
{
	ZDATA
	CPtr<CRPGUniform> pUniform;
	CPtr<CTRndModel> pModelActive;
	CPtr<CTRndModel> pModelInactive;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUniform); f.Add(3,&pModelActive); f.Add(4,&pModelInactive); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EItemSubType
{
	SUBTYPE_NONE = 0,
	SUBTYPE_HEAVY,
	SUBTYPE_PISTOL,
	SUBTYPE_KNIFE,
	SUBTYPE_GRENADE_SMALL,
	SUBTYPE_GRENADE_LARGE,
	SUBTYPE_THROWING_KNIFE,
	SUBTYPE_FIRST_AID,
	SUBTYPE_ENGINEERING,
	SUBTYPE_MINE_DETECTOR,
	SUBTYPE_AMMO_PISTOL,
	SUBTYPE_AMMO_SMG,
	SUBTYPE_AMMO_RIFLE,
	SUBTYPE_AMMO_MG,
	SUBTYPE_AMMO_HEAVY,
	//...
	N_SUBTYPES
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECameraType
{
	CAMERA_NORMAL = 0,
	CAMERA_SLOT,
	CAMERA_RELOADBUTTON,
	CAMERA_MAX_VALUE
};
struct SCameraParams
{
	float fYaw;
	float fPitch;
	float fRoll;
	float fDistance;
	CVec3 vAnchor;
	float fFOV;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIHint;       // DataMisc.h (Hints table) -- CRPGItem::pHint record ref
class CRPGStoreItem; // defined below -- CRPGStoreItem::Import installs itself on pItem->pStoreItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGItem: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGItem);
public:
	int nWeight; // in grams
	int nSubTypePriority;
	EItemSubType subType;
	CTPoint<int> sSize;
	CPtr<CString> pToolTip;
	CPtr<CString> pName;
	CPtr<CString> pDescription;
	CPtr<CTRndModel> pModel;
	CPtr<CTRndModel> pModelActive;
	CPtr<CTRndModel> pModelInactive;
	vector<SUniformItem> looks;
	SCameraParams sCameras[CAMERA_MAX_VALUE];
	CPtr<CPlacableObject> pPlaceObj;
	CPtr<CDBRecord> pSuccessor;
	CPtr<CRPGArmor> pRPGArmor;
	CPtr<CTEffect> pDestructionEffect;
	// retail CRPGItem::operator& @0x42a5d0 tags 0x12/0x13/0x15 (Import @0x42a800 columns per oracle
	// s2_dbimport.h:2727-2729):
	CPtr<CUIHint> pHint;			// tag 18, column "HintID" -- the item's UI hint record
	CPtr<CRPGStoreItem> pStoreItem;	// tag 19, NOT column-fed: CRPGStoreItem::Import @0x428570 installs itself here
	bool bPlaceInHand;   // retail CRPGItem @+0xc0 (operator& tag 20): whether the item is shown in-hand when active
	int nCost = 0;					// tag 21, column "Cost" -- the item's store price
	//
	CTRndModel* GetItemModel( bool bActive, CRPGUniform *pUniform );
	virtual void Import();

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGStoreItem: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGStoreItem);
public:
	ZDATA_(CDBRecord)
	int nRating;
	float fQuantity;
	CPtr<CSide> pSide;
	CPtr<CRPGItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nRating); f.Add(3,&fQuantity); f.Add(4,&pSide); f.Add(5,&pItem); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EStoreWeaponType
{
	SWT_OTHER,
	SWT_PISTOL,
	SWT_RIFLE,
	SWT_SUB_MACHINE_GUN,
	SWT_HEAVY_WEAPON,
	SWT_COLD_STEEL,
	SWT_GRENADE,
	SWT_PK_WEAPON
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGWeaponType: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGWeaponType);
public:
	ZDATA_(CDBRecord)
	int nSkillIndex;
	float fMovePenalty;
	float fCrawlBonus;
	float fCrouchBonus;
	bool bTwoHanded;
	CPtr<CString> pName;
	CPtr<CAISound> pAISound;
	CPtr<CAISound> pBurstAISound;
	int nWeaponTypeID;
	EStoreWeaponType eStoreWeaponType;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nSkillIndex); f.Add(3,&fMovePenalty); f.Add(4,&fCrawlBonus); f.Add(5,&fCrouchBonus); f.Add(6,&bTwoHanded); f.Add(7,&pName); f.Add(8,&pAISound); f.Add(9,&pBurstAISound); f.Add(10,&nWeaponTypeID); f.Add(11,&eStoreWeaponType); f.Add(12,&nPrepareCost); f.Add(13,&nHandlingEasyRange); f.Add(14,&nHandlingMediumRange); f.Add(15,&fMeleePenalty); return 0; }
	int nPrepareCost = 0;
	int nHandlingEasyRange = 0;
	int nHandlingMediumRange = 0;
	float fMeleePenalty = 0.0f;
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGGrenade;
class CRPGAmmo: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGAmmo);
public:
	int nBulletType;
	float fRangeMod;
	int nDmgMin, nDmgMax;
	float fUnitWeight;
	CPtr<CString> pName;
	CPtr<CRPGGrenade> pExplosiveBullet;
	int nAmmoGroup;
	EAmmoColor color;
	int nUnconsciousProbability;
	// retail CRPGAmmo @+0x38/+0x3c (Import @0x428900 columns "Calibr"/"Weight", serialize tags
	// 12/13 @0x429ad0): bullet caliber and bullet weight; they feed the corpse-push impulse
	// coefficient in CWeaponItem::CreateNewAttackPortion (retail @0x2a09e0).
	float fCalibr;
	float fWeight;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGClip: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGClip);
	ZDATA
	ZPARENT( CDBRecord );
public:
	int nQuantity;
	CPtr<CRPGItem> pItem;
	int nAmmoGroup;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nQuantity); f.Add(4,&pItem); f.Add(5,&nAmmoGroup); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail NDb::EWeaponType (Game.pdb, 15 values). The dev enum was MISSING WT_MACHETE (=6), which
// shifted every following value down by one vs. the retail game.db ints (CAnimWeaponType::type is
// loaded from the binary DB/saves as a raw int at tag 6): dev WT_MACHINE_GUN==6 matched retail
// MACHETE records, dev WT_RLAUNCHER==7 matched retail MACHINE_GUN records, etc. Values now match
// retail 1:1 (PDB: MACHETE=6, MACHINE_GUN=7, RLAUNCHER=8, MINE_DETECTOR=9, PLAZMAGUN=10,
// PK_PLAZMAGUN=11, BOSS_PLAZMAGUN=12, TERROR_SHOOTER=13, TERROR_SPECIAL_GUN=14).
enum EWeaponType
{
	WT_DEFAULT,
	WT_PISTOL,
	WT_RIFLE,
	WT_SUB_MACHINE_GUN,
	WT_KNIFE,
	WT_KATANA,
	WT_MACHETE,
	WT_MACHINE_GUN,
	WT_RLAUNCHER,
	WT_MINE_DETECTOR,
	WT_PLAZMAGUN,
	WT_PK_PLAZMAGUN,
	WT_BOSS_PLAZMAGUN,
	WT_TERROR_SHOOTER,
	WT_TERROR_SPECIAL_GUN,
};
inline bool IsMeleeWeapon( EWeaponType type ) { return type == WT_DEFAULT || type == WT_KNIFE || type == WT_KATANA; }
enum EShootMode
{
	SM_Snap = 0,
	SM_Aimed,
	SM_Careful,
	SM_ShortBurst,
	SM_LongBurst,
	SM_Snipe,

	SM_MAXVALUE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimWeaponType: public CDBRecord
{
	OBJECT_BASIC_METHODS(CAnimWeaponType);
public:
	ZDATA_(CDBRecord)
	CVec3 crawl;
	CVec3 crouch;
	CVec3 stand;
	float fMinDistance;
	EWeaponType type;
	bool bAimedStrafe;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&crawl); f.Add(3,&crouch); f.Add(4,&stand); f.Add(5,&fMinDistance); f.Add(6,&type); f.Add(7,&bAimedStrafe); f.Add(8,&fLeftHandShift); f.Add(9,&nBulletDelay); return 0; }
	float fLeftHandShift = 0.0f;
	int nBulletDelay = 0;
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGWeapon: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGWeapon);
	CPtr<CModel> pRollModel;
public:
	//
	int nInitialVelocity, nReloadAP, nWeight;
	int nShotAP, nTargetingAP;
	int nRecoil;
	int nRoF;
	int nDamageMod;
	CPtr<CRPGWeaponType> pWeaponType;
	CPtr<CRPGAmmo> pAmmo;
	CPtr<CRPGItem> pItem;
	CPtr<CSound> pSound, pSoundBurst, pSoundReload, pSoundStartBurst, pSoundFinishBurst, pSoundCycleBurst;
	bool bScope; // ����������� ������
	string szAnimName;
	int nQuality;
	CPtr<CTEffect> pShotEffect;
	CPtr<CAnimWeaponType> pAnimWeaponType;
	EWeaponType eWeaponType;
	float fTrailSpeed;
	CPtr<CTRndModel> pTrailEffect;
	CPtr<CTEffect> pTrailParticle;
	bool shootModes[SM_MAXVALUE];
	bool bBazookaLogic;
	CPtr<CRPGClip> pInnerClip;
	int nInnerClipAmmoQuantity;
	int nPanzerkleinType;
	int nMinRange, nMaxRange;
	// v1.1 retail tail (Jan03 lacks these; PDB CRPGWeapon +0x9c..+0xac). nBPM ("BPM" column,
	// bullets per MINUTE) is the GetNextBulletTime divisor -- NOT nRoF, which is the full-burst
	// bullet count (nRoF/6 = short burst).
	int nBPM = 0;            // +0x9c "BPM"
	int nDamageModMax = 0;   // +0xa0 "DamageModMax"
	float fSilencer = 0.0f;  // +0xa4 "Silencer"
	int nAIRating = 0;       // +0xa8 "AIRating"
	int nShotEffectType = 0; // +0xac "ShotEffectType"
	// v1.2: NEW int appended to the record (retail +0xb0, "ShotsInOne" column) -- bullets fired
	// by ONE trigger pull; >1 makes GetNextBulletTime/CheckBurst fire all pellets as one shot.
	int nShotsInOne = 1;
	//
	CModel* GetModel();
	int operator&( CStructureSaver &f );
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EItemPlace
{
	BELT_L1 = 0,
	BELT_R1,
	BELT_M1,
	BELT_MEDIUM_L1,
	BELT_MEDIUM_R1,
	BELT_MEDIUM_L2,
	BELT_MEDIUM_R2,
	WAIST_BELT_L1,
	WAIST_BELT_R1,
	N_ITEM_PLACES
};
class CRPGUniform: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGUniform);
public:
	CPtr<CTRndModel> pCapModel;
	CPtr<CTRndModel> pBackpackModel;
	EItemSubType subTypes[N_ITEM_PLACES];
	vector< CPtr<CTRndModel> > fixedModels;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MAX_GRENADE_EXPLOSION_TYPE = 4;
const int N_MAX_GRENADE_SOUND_TYPE = 4;
class CRPGGrenade: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGGrenade);
public:
	template<class T, int N>
	struct SSwitch
	{
		CPtr<T> p[N];
		int operator&( CStructureSaver &f ) { for ( int k = 0; k < N; ++k ) f.Add( 1, &p[k], k + 1 );  return 0; }
	};
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	// �������� �����	
	int nWaveNumber;
	float fWaveDmgMin;
	float fWaveDmgMax;
	int nCriticalProbability;
	int nCriticalDifficulty;
	float fStructureDamageCoeff;
	float fWaveRadius;
	// �������
	int nFragmentNumber;
	int nFragmentAPA;
	int nFragmentDmgMin;
	int nFragmentDmgMax;
	//
	//CPtr<CSound> pSound;
	SSwitch<CTEffect, N_MAX_GRENADE_EXPLOSION_TYPE> pEffect;
	SSwitch<CTSound, N_MAX_GRENADE_SOUND_TYPE> pSound;
	int nMaxDelay;
	int nQuality;
	CPtr<CRPGWeaponType> pWeaponType;
	int nPanzerkleinWeapon;
	float fDecalRadius;
	// retail CRPGGrenade @+0x74 (operator& @0x41adb0 tag 0x15, Import column "FragmentRange" @0x8d38d8):
	// splinter max flight range in grid units (ExplodeFragments uses fFragmentRange * FP_GRID_STEP)
	float fFragmentRange;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nWaveNumber); f.Add(4,&fWaveDmgMin); f.Add(5,&fWaveDmgMax); f.Add(6,&nCriticalProbability); f.Add(7,&nCriticalDifficulty); f.Add(8,&fStructureDamageCoeff); f.Add(9,&fWaveRadius); f.Add(10,&nFragmentNumber); f.Add(11,&nFragmentAPA); f.Add(12,&nFragmentDmgMin); f.Add(13,&nFragmentDmgMax); f.Add(14,&pEffect); f.Add(15,&pSound); f.Add(16,&nMaxDelay); f.Add(17,&nQuality); f.Add(18,&pWeaponType); f.Add(19,&nPanzerkleinWeapon); f.Add(20,&fDecalRadius); f.Add(21,&fFragmentRange); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGEngGrenade - engineer/PK grenade (release table 0x7b RPGEngGrenades). Sibling of CRPGGrenade;
// reuses its SSwitch<,4> effect/sound multiplexers. Tags/columns verbatim from the release decompile.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGEngGrenade: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGEngGrenade);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	CRPGGrenade::SSwitch<CTEffect, N_MAX_GRENADE_EXPLOSION_TYPE> pEffect;
	CRPGGrenade::SSwitch<CTSound, N_MAX_GRENADE_SOUND_TYPE> pSound;
	int nSkillReq;
	int nStartNWave;
	int nDeltaWave;
	float fWaveRadius;
	float fDeltaRadius;
	float fDamageModifier;
	float fFragDmgModifier;
	int nAPAModifier;
	int nCriticalProbability;
	int nCriticalDifficulty;
	float fStructureDamageCoeff;
	int nRequiredPerkID;
	int nMaxDelay;
	int nQuality;
	CPtr<CRPGWeaponType> pWeaponType;
	int nPanzerkleinWeapon;
	float fDecalRadius;
	float fFragmentRange;
	float fStartWaveDamage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&pEffect); f.Add(4,&pSound); f.Add(5,&nSkillReq); f.Add(6,&nStartNWave); f.Add(7,&nDeltaWave); f.Add(8,&fWaveRadius); f.Add(9,&fDeltaRadius); f.Add(10,&fDamageModifier); f.Add(11,&fFragDmgModifier); f.Add(12,&nAPAModifier); f.Add(13,&nCriticalProbability); f.Add(14,&nCriticalDifficulty); f.Add(15,&fStructureDamageCoeff); f.Add(16,&nRequiredPerkID); f.Add(17,&nMaxDelay); f.Add(18,&nQuality); f.Add(19,&pWeaponType); f.Add(20,&nPanzerkleinWeapon); f.Add(21,&fDecalRadius); f.Add(22,&fFragmentRange); f.Add(23,&fStartWaveDamage); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGFirstAid: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGFirstAid);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nQuantity;
	EFirstAidEffect effect;
	int nRequiedSkill;
	int nSkillModifier;
	int nDuration;
	float fPower;
	int nTotalHealVP; // ���������� �����, ������� ����� �������� ���� ��������
	int nAPToUse;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nQuantity); f.Add(4,&effect); f.Add(5,&nRequiedSkill); f.Add(6,&nSkillModifier); f.Add(7,&nDuration); f.Add(8,&fPower); f.Add(9,&nTotalHealVP); f.Add(10,&nAPToUse); f.Add(11,&nRequiredPerkID); return 0; }
	int nRequiredPerkID = -1;

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGMeleeWeapon: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGMeleeWeapon);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nDmgMin;
	int nDmgMax;
	int nToHitBonus;
	int nCriticalBonus;
	int nMinAP;
	int nMaxAP;
	CPtr<CRPGWeaponType> pWeaponType;
	CPtr<CAnimWeaponType> pAnimWeaponType;
	bool bThrowing;
	int nUnconsciousProbability;
	int nPanzerkleinType;
	EWeaponType eWeaponType;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nDmgMin); f.Add(4,&nDmgMax); f.Add(5,&nToHitBonus); f.Add(6,&nCriticalBonus); f.Add(7,&nMinAP); f.Add(8,&nMaxAP); f.Add(9,&pWeaponType); f.Add(10,&pAnimWeaponType); f.Add(11,&bThrowing); f.Add(12,&nUnconsciousProbability); f.Add(13,&nPanzerkleinType); f.Add(14,&eWeaponType); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGMineDetector: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGMineDetector);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGMine: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGMine);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nAPToSet;
	CPtr<CRPGGrenade> pExplosion;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nAPToSet); f.Add(4,&pExplosion); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGTool: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGTool);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nCharges;
	int nNeededEngSkill;
	CPtr<NDb::CDBPerk> pNeededPerk;
	bool bCanUseForMineCleaning;
	int nSkillModifForMineCleaning;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nCharges); f.Add(4,&nNeededEngSkill); f.Add(5,&pNeededPerk); f.Add(6,&bCanUseForMineCleaning); f.Add(7,&nSkillModifForMineCleaning); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGKey: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGKey);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nKeyID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nKeyID); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGClass: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGClass);
public:
	ZDATA_(CDBRecord)
	int nPerkTreeID;
	SRPGSkills skills;
	////
	CPtr<CString> pToolTip;
	CPtr<CUITexture> pIcon;
	CPtr<CUITexture> pIconDisabled;
	CPtr<CUIContainer> pPerksPanel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nPerkTreeID); f.Add(3,&skills); f.Add(4,&pToolTip); f.Add(5,&pIcon); f.Add(6,&pIconDisabled); f.Add(7,&pPerksPanel); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGBaseValue: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGBaseValue);
public:
	SRPGSkills skills;
	int nBaseXP;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SItemAssign
{
	ZDATA
	int nQuantity;
	CPtr<CDBRecord> pItem;
	CPtr<CRPGAmmo> pAmmo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nQuantity); f.Add(3,&pItem); f.Add(4,&pAmmo); f.Add(5,&pDifficulty); return 0; }
	CPtr<CDBDifficulty> pDifficulty;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScript: public CDBRecord
{
	OBJECT_BASIC_METHODS(CScript);
public:
	ZDATA_(CDBRecord)
	string strCode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&strCode); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPanzerklein;
class CRPGPers: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGPers);
public:
	enum ECameraType
	{
		CAMERA_FACEGEN,
		CAMERA_PORTRAIT,
		CAMERA_MAXVALUE
	};

	ZDATA_(CDBRecord)
	int nRPGPersID;
	string szUserName;
	////
	CPtr<CString> pName;
	CPtr<CTSound> pSoundHit;
	CPtr<CTSound> pSoundDeath;
	CPtr<CTRndModel> pModel;
	CPtr<CRPGUniform> pUniform;
	CPtr<CComplexHead> pHead;
	CPtr<CPanzerklein> pPanzerklein;
	SCameraParams sFaceGenCamera;
	SCameraParams sPortraitCamera;
	////
	CPtr<CSide> pSide;
	CPtr<CNationality> pNationality;
	CPtr<CRPGPers> pDefaultWearsPanzerklein;
	CPtr<CRPGClass> pClass;
	CPtr<CRPGWeapon> pWeapon;
	vector<SItemAssign> items;
	CPtr<CRPGBaseValue> pBaseValue;
	////
	vector< CPtr<CScript> > scripts;
	bool bIsFemale;
	int nVoice;
	bool bCanBeListed;   // release-added: include this persona in the CharGen model-override roll (NUI::GetPers @0x1b5840)
	// release-added recruit-menu / biography fields (retail CRPGPers +0xb4..0xc0; operator& @0x41f490
	// tags 0x17-0x1a; Import @0x429540 columns PhotoID/BiographyID/CharacteristicsID/CanHired):
	CPtr<CUITexture> pPhoto;			// "PhotoID" -- the biography-panel portrait photo (CUnitBiographyPanel "image")
	CPtr<CString> pBiography;			// "BiographyID" -- biography prose (seeds NRPG::CUnit::pBiography in the CUnit ctor @0x2bc980)
	CPtr<CString> pCharacteristics;		// "CharacteristicsID" -- the bio panel's "characteristics" text block
	bool bCanHired = false;				// "CanHired" -- THE recruit-roster filter (NRPG::AddTeamMngPerses @0x29adb0 reads +0xc0)
	// release-added loot chests (retail CRPGPers::operator& @0x41f490 tags 0x1c/0x1d; Import @0x429540):
	CPtr<CTRPGChest> pHandWeapon;		// "WeaponInHand" -- chest template rolled for the unit's in-hand item (SMapUnit::pInHandItem)
	CPtr<CTRPGChest> pBackpackWeapon;	// "WeaponInBackpack" -- chest template rolled into the unit's backpack (SMapUnit::pBackpack)
	// retail CRPGPers::pAcksHolder (+0xd0; operator& @0x41f490 tag 0x1f; Import @0x429540 "AckUnit"):
	// the VOICE-HOLDER persona whose Acks rows this persona barks with. In the Steam game.db (chunk-
	// verified: RPGPers intCols[25]="AckUnit", full 27-value rows) 499 of 728 personas (all generic
	// enemies/allies/NPCs) have their ack rows keyed ONLY via this link (e.g. every Germ_* soldier
	// -> pers 911, which owns 20 Acks rows); without it those units have no barks at all.
	CPtr<CRPGPers> pAcksHolder;			// "AckUnit" -- ack rows donor (NRPG::CUnit::GetAckHolder @0x2ba8f0 non-hero path)
	// retail CRPGPers +0xcc/+0xd4 (operator& @0x41f490 tags 0x1e/0x20; Import @0x429540 columns
	// "LongNameID"/"LongBurstSnd", oracle s2_crpgpers.h:206-208):
	CPtr<CString> pLongName;			// "LongNameID" -- the persona's long display name
	CPtr<CSound> pLongBurstSnd;			// "LongBurstSnd" -- per-persona long-burst voice sound
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nRPGPersID); f.Add(3,&szUserName); f.Add(4,&pName); f.Add(5,&pSoundHit); f.Add(6,&pSoundDeath); f.Add(7,&pModel); f.Add(8,&pUniform); f.Add(9,&pHead); f.Add(10,&pPanzerklein); f.Add(11,&sFaceGenCamera); f.Add(12,&sPortraitCamera); f.Add(13,&pSide); f.Add(14,&pNationality); f.Add(15,&pDefaultWearsPanzerklein); f.Add(16,&pClass); f.Add(17,&pWeapon); f.Add(18,&items); f.Add(19,&pBaseValue); f.Add(20,&scripts); f.Add(21,&bIsFemale); f.Add(22,&nVoice); f.Add(0x17,&pPhoto); f.Add(0x18,&pBiography); f.Add(0x19,&pCharacteristics); f.Add(0x1a,&bCanHired); f.Add(0x1b,&bCanBeListed); f.Add(0x1c,&pHandWeapon); f.Add(0x1d,&pBackpackWeapon); f.Add(0x1e,&pLongName); f.Add(0x1f,&pAcksHolder); f.Add(0x20,&pLongBurstSnd); return 0; }	// retail (CRPGPers::operator& @0x41f490) tag map: pPhoto=0x17, pBiography=0x18, pCharacteristics=0x19, bCanHired=0x1a, bCanBeListed=0x1b (now all read; previously reading bCanBeListed at 23 decoded the pPhoto object-ref chunk as a bool); 0x1e=pLongName, 0x1f=pAcksHolder, 0x20=pLongBurstSnd -- FULL retail table now covered
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECriticalLocation
{
	CL_HEAD = 0,
	CL_TORSO,
	CL_ARMS,
	CL_LEGS,
	CL_ANY,
	N_CL
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECritical
{
	C_DEATH = 0,
	C_AP_REDUCTION,
	C_BLIND,
	C_WEAPONSKILL_REDUCTION,
	C_VP,
	C_MOTIONLESS,
	C_ENCUMBRANCE,
	C_ACCIDENTAL_SHOT,
	C_LOST_WEAPON,
	C_IDLE_HAND,
	C_STUN,
	C_DAMAGE_WEAPON,
	C_PATIENT,
	C_DEAF,
	C_BLEEDING,
	C_PANZERKLEIN_AXIS,
	C_PANZERKLEIN_ALLIES,
	C_PANZERKLEIN_TERRORS,
	C_PANZERKLEIN_BROKEN,
	C_PANZERKLEIN_AXIS_SOLDIER,
	C_PANZERKLEIN_AXIS_ENGINEER,
	C_PANZERKLEIN_ALLIES_SCOUT,
	C_PANZERKLEIN_ALLIES_SNIPER,
	C_PANZERKLEIN_TERRORS_MEDIC,
	C_PANZERKLEIN_TERRORS_HWG,
	C_NONE,
	N_CRIT_TYPES
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGCritical: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGCritical);
public:
	ZDATA_(CDBRecord)
	ECritical type;
	ECriticalLocation hl;
	int nWeight;
	int nRange;
	int nMinDuration;
	int nMaxDuration;
	float fValue;
	string szName;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&type); f.Add(3,&hl); f.Add(4,&nWeight); f.Add(5,&nRange); f.Add(6,&nMinDuration); f.Add(7,&nMaxDuration); f.Add(8,&fValue); f.Add(9,&szName); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SToHitConstants
{
	ZDATA
	int nMaxShotsRepeat;
	int nMaxMoveBonus;
	float fFirstRoundCoeff;
	int nMaxBurstStabilize;
	int nSnipingCoeff;
	int nAreaCoverCoeff;
	int nSMaxMove;
	float fGrenadeBaseCoeff;
	float fGrenadeSTRCoeff;
	float fGravity;
	float fGrenadeBaseWeight;
	float fMaxWeightRelation;
	float fICModifier;
	int nStrikeAddition;
	int nMaxAdditionalStrikes;
	int nMaxAPonStrike;
	float fBackSkillMult;
	float fStanceMult;
	int nBaseMeleeCritChance;
	int nBaseDamage;
	int nStrengthNormalize;
	float fBackCriticalMult;
	float fAttackerMult;
	float fDefenderMult;
	int nMeleeToHitScaling;
	int nBaseMeleeToHit;
	int nGrenadeWeightScaling;
	int nGrenadeWeightScalingBase;
	int nMaxGrenadeWeightDifference;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nMaxShotsRepeat); f.Add(3,&nMaxMoveBonus); f.Add(4,&fFirstRoundCoeff); f.Add(5,&nMaxBurstStabilize); f.Add(6,&nSnipingCoeff); f.Add(7,&nAreaCoverCoeff); f.Add(8,&nSMaxMove); f.Add(9,&fGrenadeBaseCoeff); f.Add(10,&fGrenadeSTRCoeff); f.Add(11,&fGravity); f.Add(12,&fGrenadeBaseWeight); f.Add(13,&fMaxWeightRelation); f.Add(14,&fICModifier); f.Add(15,&nStrikeAddition); f.Add(16,&nMaxAdditionalStrikes); f.Add(17,&nMaxAPonStrike); f.Add(18,&fBackSkillMult); f.Add(19,&fStanceMult); f.Add(20,&nBaseMeleeCritChance); f.Add(21,&nBaseDamage); f.Add(22,&nStrengthNormalize); f.Add(23,&fBackCriticalMult); f.Add(24,&fAttackerMult); f.Add(25,&fDefenderMult); f.Add(26,&nMeleeToHitScaling); f.Add(27,&nBaseMeleeToHit); f.Add(28,&nGrenadeWeightScaling); f.Add(29,&nGrenadeWeightScalingBase); f.Add(30,&nMaxGrenadeWeightDifference); f.Add(31,&nMaxWeaponAdaptation); f.Add(32,&fWeaponAdaptationMult); return 0; }
	int nMaxWeaponAdaptation = 0;
	float fWeaponAdaptationMult = 0;

	SToHitConstants()
	{
		nMaxShotsRepeat = 0;
		nMaxMoveBonus = 0;
		fFirstRoundCoeff = 0;
		nMaxBurstStabilize = 0;
		nSnipingCoeff = 0;
		nAreaCoverCoeff = 0;
		nSMaxMove = 0;
		fGrenadeBaseCoeff = 0;
		fGrenadeSTRCoeff = 0;
		fGravity = 0;
		fGrenadeBaseWeight = 0;
		fMaxWeightRelation = 0;
		fICModifier = 0;
		nStrikeAddition = 0;
		nMaxAdditionalStrikes = 0;
		nMaxAPonStrike = 0;
		fBackSkillMult = 0;
		fStanceMult = 0;
		nBaseMeleeCritChance = 0;
		nBaseDamage = 0;
		nStrengthNormalize = 0;
		fBackCriticalMult = 0;
		fAttackerMult = 0;
		fDefenderMult = 0;
		nMeleeToHitScaling = 0;
		nBaseMeleeToHit = 0;
		nGrenadeWeightScaling = 0;
		nGrenadeWeightScalingBase = 0;
		nMaxGrenadeWeightDifference = 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGToHit: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGToHit);
public:
	ZDATA_(CDBRecord)
	SToHitConstants constants;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&constants); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAISoundConstants
{
	ZDATA
	int nPrecisePositionRadius;
	int nExitRadius;
	int nBaseProbability;
	int nDistanceCoeff;
	int nLoudSound;
	float fHideCoeff;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPrecisePositionRadius); f.Add(3,&nExitRadius); f.Add(4,&nBaseProbability); f.Add(5,&nDistanceCoeff); f.Add(6,&nLoudSound); f.Add(7,&fHideCoeff); return 0; }

	SAISoundConstants()
	{
		nPrecisePositionRadius = 0;
		nExitRadius = 0;
		nBaseProbability = 0;
		nDistanceCoeff = 0;
		nLoudSound = 0;
		fHideCoeff = 1;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGAISoundConstants: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGAISoundConstants);
public:
	ZDATA_(CDBRecord)
	SAISoundConstants constants;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&constants); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInterruptsConstants
{
	ZDATA
	int nBackInterruptsBase;
	int nInterruptsBase;
	int nMissedShotInterruptsBase;
	float fAPInterruptReduction;
	int nMinInterruptAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBackInterruptsBase); f.Add(3,&nInterruptsBase); f.Add(4,&nMissedShotInterruptsBase); f.Add(5,&fAPInterruptReduction); f.Add(6,&nMinInterruptAP); return 0; }

	SInterruptsConstants()
	{
		nBackInterruptsBase = 0;
		nInterruptsBase = 0;
		nMissedShotInterruptsBase = 0;
		fAPInterruptReduction = 0;
		nMinInterruptAP = 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGInterruptsConstants: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGInterruptsConstants)
public:
	ZDATA_(CDBRecord)
	SInterruptsConstants constants;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&constants); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkeleton;
class CExplosion;
const int N_PK_SPECIAL_WEAPONS = 10;	// game.db AllowWeaponType1..10 / PDB bool[10] / Import @0x4297e0 push 0xa
class CPanzerklein: public CDBRecord
{
	OBJECT_BASIC_METHODS( CPanzerklein );
public:
	typedef bool TBoolArray[ N_PK_SPECIAL_WEAPONS ];
	ZDATA
	ZPARENT( CDBRecord );
	CPtr<CRPGPers> pPers;
	int nRicochetProb;
	int nMaxVP;
	int nAddMoveAP;
	float fAddCoverIgnore;
	float fSensorRange;
	float fRegenerationValue;
	float fCriticalResist;
	CPtr<CRPGArmor> pArmor;
	float fEncumbranceKoeff;
	CPtr<CRPGGrenade> pSelfExplosion;
	TBoolArray bAllowWeaponType;
	CPtr<CRPGBaseValue> pChangeValues;
	CPtr<CRPGItem> pLeftHandItem;
	int nGrenadeStrength;
	bool bHasNoHead;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&pPers); f.Add(4,&nRicochetProb); f.Add(5,&nMaxVP); f.Add(6,&nAddMoveAP); f.Add(7,&fAddCoverIgnore); f.Add(8,&fSensorRange); f.Add(9,&fRegenerationValue); f.Add(10,&fCriticalResist); f.Add(11,&pArmor); f.Add(12,&fEncumbranceKoeff); f.Add(13,&pSelfExplosion); f.Add(14,&bAllowWeaponType); f.Add(15,&pChangeValues); f.Add(16,&pLeftHandItem); f.Add(17,&nGrenadeStrength); f.Add(18,&bHasNoHead); f.Add(19,&bCanHide); f.Add(20,&bSingleSlot); f.Add(21,&pEngineSound); f.Add(22,&pStepSound); return 0; }
	bool bCanHide = false;
	bool bSingleSlot = false;
	CPtr<CSound> pEngineSound;
	CPtr<CTSound> pStepSound;
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNationality: public CDBRecord
{
	OBJECT_BASIC_METHODS(CNationality);
public:
	// Retail CNationality schema (operator& @0x418580, size 0x3c). The release DROPPED the per-nationality
	// customMaleHeads/customFemaleHeads vectors (heads are now gathered from the global CComplexHead table by
	// gender) and ADDED nFaceGenTemplate + biographies + char-gen/custom-head backgrounds. Converging this is
	// required to read the retail 36MB game.db correctly: our old tag 8 (a vector) MISREAD the retail record's
	// int nFaceGenTemplate at that tag.
	ZDATA_(CDBRecord)
	CPtr<CString> pToolTip;
	CPtr<CUITexture> pFlag;
	CPtr<CUITexture> pIconNormal;
	CPtr<CUITexture> pIconDisabled;
	CPtr<CComplexHead> pMaleHead;
	CPtr<CComplexHead> pFemaleHead;
	int nFaceGenTemplate;            // 3D backdrop render template for the face-gen / hero-select screen
	CPtr<CString> pMaleBiography;
	CPtr<CString> pFemaleBiography;
	CPtr<CUITexture> pCharGenBackground;
	CPtr<CUITexture> pCustomHeadBackground;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pToolTip); f.Add(3,&pFlag); f.Add(4,&pIconNormal); f.Add(5,&pIconDisabled); f.Add(6,&pMaleHead); f.Add(7,&pFemaleHead); f.Add(8,&nFaceGenTemplate); f.Add(9,&pMaleBiography); f.Add(10,&pFemaleBiography); f.Add(11,&pCharGenBackground); f.Add(12,&pCustomHeadBackground); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSide: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSide);
public:
	enum EPersClass
	{
		MEDIC,
		SCOUT,
		SNIPER,
		SOLDIER,
		ENGINEER,
		GRENADIER,

		CLASS_MAXVALUE
	};

	ZDATA_(CDBRecord)
	int nGlobalMapID;
	int nHeroSelectTemplate;
	CPtr<CString> pName;
	CPtr<CNationality> pNationality1;
	CPtr<CNationality> pNationality2;
	CPtr<CNationality> pNationality3;
	vector<CPtr<CRPGPers> > malePersesSet;
	vector<CPtr<CRPGPers> > femalePersesSet;
	vector<CPtr<CRPGPers> > defaultPersesSet;
	// retail CSide medals list (save tag 11): NOT column-fed on CSide -- each CMedal record installs
	// itself here from CMedal::Import @0x42a110 (dedup push into its side's medals).
	vector<CPtr<CMedal> > medals;
	//// Interface
	CPtr<CUITexture> pESCMenuBackground;
	CPtr<CUITexture> pChapterMapInfoBackground;	// retail +0x54, save tag 13; Import column "UICMInfoBackgroundID" -- chapter-map info panel background
	CPtr<CUITexture> pCluePaperBackground;		// retail +0x6c, save tag 19; Import column "UICluePaperBackgroundID" -- clue-paper background
	//// GlobalView
	CPtr<CUITexture> pBaseFlag;
	CPtr<CUITexture> pBaseFlagActive;
	CPtr<NDb::CDBDialogPers> pDialogHero;
	CPtr<CUIContainer> pMedalPaperContainer;	// release-new CSide field (retail +0x60, save tag 14): the "medal paper" UI template loaded by NGame::CShowMedalInterface::Initialize (iShowMedal convergence). Additive -- fills the previously-skipped tag 14 slot, existing 2..17 numbering undisturbed.
	CPtr<CUIContainer> pKIAPaper;	// release-new CSide field (retail +0x70, save tag 18): the per-side "killed in action" name-plate template the recruit menu loads into its CKIAPanel (CTeamMngUI::ProcessMessage @0x2498f0 reads side+0x70). Not imported by retail CSide::Import @0x42a240 -- populated from the game.db chunk stream (tag 18) only. Additive.
	vector<CPtr<CString> > defaultPersToolTipsSet;	// release-new CSide field (retail +0x78, save tag 0x14=20): the 6 per-preset (nationality x gender) class-description tooltips shown on the HeroMenu portraits. POPULATED FROM THE STEAM game.db BY Import() below (the v1 columnar DB binds by named column, NOT by operator& tags); the f.Add(20,..) tag is parity / savegame round-trip only.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nGlobalMapID); f.Add(3,&nHeroSelectTemplate); f.Add(4,&pName); f.Add(5,&pNationality1); f.Add(6,&pNationality2); f.Add(7,&pNationality3); f.Add(8,&malePersesSet); f.Add(9,&femalePersesSet); f.Add(10,&defaultPersesSet); f.Add(11,&medals); f.Add(12,&pESCMenuBackground); f.Add(13,&pChapterMapInfoBackground); f.Add(14,&pMedalPaperContainer); f.Add(15,&pBaseFlag); f.Add(16,&pBaseFlagActive); f.Add(17,&pDialogHero); f.Add(18,&pKIAPaper); f.Add(19,&pCluePaperBackground); f.Add(20,&defaultPersToolTipsSet); return 0; }	// FULL retail CSide tag table (release @0x41dea0): 2..10 record scalars/rosters, 11=medals, 12=pESCMenuBackground, 13=pChapterMapInfoBackground, 14=pMedalPaperContainer, 15=pBaseFlag, 16=pBaseFlagActive, 17=pDialogHero, 18=pKIAPaper, 19=pCluePaperBackground, 20=defaultPersToolTipsSet (last chunk).
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGMeleeWeapon* GetMeleeWeapon( int nID );
CRPGToHit* GetToHitConstants( int nID );
CRPGAISoundConstants *GetAISoundConstants( int nID );
CRPGInterruptsConstants *GetInterruptsConstants( int nID );
CSound* GetSound( CTSound *p );
CRPGItem* GetRPGItem( int nID );
CAnimWeaponType* GetAnimWeaponType( int nAnimFlags );
int WeaponTypeToAnimFlags( EWeaponType type, bool bActive, bool bPK );
CRPGWeapon* GetWeapon( int nID );
CRPGPers* GetPers( int nID );
CRPGArmor* GetArmor( int nID );
CRPGDmgToArmor* GetDBDmg2Armor( int nID );
CRPGGrenade *GetRPGGrenade( int nID );
CRPGCritical *GetDBCritical( int nID );
CRPGMaterial *GetRPGMaterial( int nID );
CScript *GetDBScript( int nID );
CSide* GetDBSide( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif