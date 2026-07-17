#ifndef __RPGUNIT_H_
#define __RPGUNIT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GSkeleton.h"
#include "../DBFormat/DataFormat.h"
#include "../DBFormat/DataRPG.h"
#include "../DBFormat/DataDifficulty.h"
#include "../DBFormat/DataMisc.h"   // NDb::CMedal (CMedalsGainer base)
#include "../Misc/RandomGen.h"      // SRandomSeed (CUnit::bindSeed)
#include "RPGMedals.h"              // NRPG::EMedalPointCases (CMedalsGainer::AddMedalPoints sink)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	enum EPose;
}
namespace NLSHead
{
	class CHeadInfo;   // CUnit::pHeadInfo (live head; full type in LSHead.h)
}
namespace NDb
{
	class CRPGChestReal;   // rolled chest loot (DataChest.h; CUnit ctor pBackpack param)
}
namespace NRPG
{
class IFirstAidItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFirstAid
{
	float fdVP;   // VP to restore per application
	int nMaxVP;   // max VP that can be healed (capped by the unit's missing VP)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IInventory;
class IInventoryItem;
class CWeaponItem;
class CMeleeWeaponItem;
class CPerksTree;
class CGlobalGame;   // CMedalsGainer::AddMedalPoints sink (held only by pointer)
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MAX_SKILL = 140;
const int N_MAX_VP = 250;
const int N_MAX_DC = 300;
////////////////////////////////////////////////////////////////////////////////////////////////////
// The {multiplier, flat add} payload of a skill modifier -- retail NRPG::SSkillModifyInfo
// (PDB: fMul @0, fAdd @4, sizeof 8). Serialized raw (8-byte DataChunk) inside SModifierHolder.
struct SSkillModifyInfo
{
	float fMul;
	float fAdd;
	SSkillModifyInfo(): fMul( 1 ), fAdd( 0 ) {}
	SSkillModifyInfo( float _fMul, float _fAdd ): fMul( _fMul ), fAdd( _fAdd ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkillModifier;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Skill cell - a dynamic value such as HP or AP.
// Retail model (@0x2bdc40 operator&, PDB sizeof 48): the Jan03 scalar fMultiplier is GONE --
// retail keeps the list of applied CSkillModifier objects ON the cell (weak CPtr refs; the
// OWNING refs live on the criticals / unit-mission modifier vectors) plus a separate XP-earned
// component (nXPValue; nMaxValue is the DERIVED, modifier-adjusted cap recomputed by Update)
// and a freeze-to-max flag.
class CDynamicSkill: public CObjectBase
{
	OBJECT_BASIC_METHODS(CDynamicSkill);
private:
	friend class CSkillModifier;   // ctor/dtor self-install via AddModifier/RemoveModifier
	ZDATA
	int nBaseValue;  // base value derived from the stat
	int nXPValue;    // whole XP-earned points (retail keeps XP separate from the cap)
	float fProgress; // progress toward the next +1
	int nMaxValue;   // modifier-adjusted cap: ROUND((nXPValue+nBaseValue)*IImul + Iadd), by Update()
	int nValue;      // current value, may be lower than nMaxValue
	vector< CPtr<CSkillModifier> > modifiers; // weak refs; Update() sweeps dead entries
	bool bFreezedToMax;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBaseValue); f.Add(3,&nXPValue); f.Add(4,&fProgress); f.Add(5,&nMaxValue); f.Add(6,&nValue); f.Add(7,&modifiers); f.Add(8,&bFreezedToMax); return 0; } // retail @0x2bdc40
public:
	// dev-compat two-arg ctor expressed in the retail member model (retail's own ctor
	// @0x2bd380 takes just the base value and leaves the cell empty; dev creation sites
	// construct full cells, so the XP part is derived from the requested set value).
	CDynamicSkill( int nSetValue = 0, int nBaseStatValue = 0 ):
		nBaseValue(nBaseStatValue), nXPValue( nSetValue - nBaseStatValue ), fProgress( 0 ),
		nMaxValue( nSetValue ), nValue( nSetValue ), bFreezedToMax( false )
	{
		if ( nXPValue < 0 )
		{
			nXPValue = 0;
			nMaxValue = nBaseValue;
			nValue = nBaseValue;
		}
	}

	void Update();                          // retail @0x2bbb20: recompute nMaxValue, sweep dead modifiers, clamp nValue down
	void AddModifier( CSkillModifier *p );  // retail @0x2bc6a0
	void RemoveModifier( CSkillModifier *p );// retail @0x2bbbe0
	void SetNewMaxValue( int nNewValue );
	void SetNewBaseValue( int nNewValue );
	void Modify( int nModif );              // retail @0x2bd1a0: clamped value-only shift
	bool Upgrade( float fAddToProgress, float fCap ); // retail @0x2bbc30: XP-based, clamped to the cap
	int  GetXPPart() const { return nXPValue; } // value gained from XP only
	void SetXPPart( int nNewXPPart ) { nValue += nNewXPPart - nXPValue; nXPValue = nNewXPPart; Update(); }

	float GetProgress() const { return fProgress; }
	int GetMaxValue() const { return nMaxValue; }  // already modifier-adjusted in the retail model
	int GetTheoreticalMax() const { return nXPValue + nBaseValue; }  // the UNmodified base+XP cap (retail reads it raw, e.g. @0x295280/@0x2c3400)
	int GetCurrentMaxValue() const { return nMaxValue; } // dev-compat alias (retail folds the modifiers into nMaxValue)
	void Reset() { nValue = nMaxValue; }
	void SetValue( int n ) { nValue = n < nMaxValue ? n : nMaxValue; } // retail @0x29b230 clamps to the cap
	void SetProgress( float p ) { fProgress = p; }
	void FreezeToMax( bool b ) { bFreezedToMax = b; }

	void SetConst( int nConstValue ) { nValue = nMaxValue = nBaseValue = nConstValue; nXPValue = 0; }

	// retail CWorld::CreateAIUnits @0x36b2b0 (inline): difficulty scaling writes the cap
	// directly from the CURRENT figure and clamps the live value to it.
	void ScaleForDifficulty( float fCoeff );

	const CDynamicSkill& operator += ( int n ) { nValue += n; if ( nValue > nMaxValue ) nValue = nMaxValue; return *this; }
	const CDynamicSkill& operator -= ( int n ) { nValue -= n; return *this; }
	// retail @0x232e0: the effective figure is frozen-to-max ? cap : min(cap, value)
	operator int () const { return ( bFreezedToMax || nValue > nMaxValue ) ? nMaxValue : nValue; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail CSkillModifier (@0x296830 operator&, PDB sizeof 24): {pSkill, fMul, fAdd}. Still RAII
// like the Jan03 SModif -- the ctor (@0x295e40) installs itself into the skill's modifier list
// via AddModifier, the dtor (@0x295ee0) detaches via RemoveModifier -- but the applied effect
// lives on the CELL (Update folds every attached modifier into nMaxValue).
class CSkillModifier: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CSkillModifier);
public:
	ZDATA
	CPtr<CDynamicSkill> pSkill;
	float fMul;
	float fAdd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSkill); f.Add(3,&fMul); f.Add(4,&fAdd); return 0; } // retail @0x296830

	CSkillModifier(): fMul( 1 ), fAdd( 0 ) {}
	CSkillModifier( CDynamicSkill *_pSkill, const SSkillModifyInfo &info ); // retail @0x295e40: self-installs
	~CSkillModifier();                                                      // retail @0x295ee0: self-detaches
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkilledObject -- the "thing that has skills" base (release re-architecture). Holds the XP
// accumulator, the per-skill CDynamicSkill cells and the per-skill XP-cap array. Serialized as a
// CUnit base subobject (operator& tags 2/3/4 == fXP/skills/cap). A plain (non-CObjectBase) class:
// CUnit supplies the single CObjectBase base. See docs/CONVERGENCE_PROGRESS.md.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkilledObject
{
public:
	ZDATA
	float fXP;                              // total experience accumulated
	vector< CObj<CDynamicSkill> > skills;   // one cell per NDb::ESkillType
	vector< int > cap;                      // per-skill XP cap (from the unit's class)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fXP); f.Add(3,&skills); f.Add(4,&cap); return 0; }

	CSkilledObject(): fXP(0) {}

	CDynamicSkill& Skills( const int eSkill ) { return *skills[eSkill]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNamedObject -- the "thing that has a name" base (release re-architecture). Serialized as a CUnit
// base subobject (the wsName string).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNamedObject
{
public:
	ZDATA
	wstring wsName;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsName); return 0; }

	const wstring& GetName() const { return wsName; }
	void SetName( const wstring &_wsName ) { wsName = _wsName; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsGainer -- per-unit medal bookkeeper (release-new CUnit base, RPGMedals.obj compiland).
// Owns one SMedalInfo per medal the unit can earn. Serialized as a CUnit base subobject
// (operator& tags 2/3/4). NOTE: the medal-award methods (GainMedalsAfterMissionEnd /
// GetGainedMedals / ThrowCheck / the side-driven ctor) require NDb::CSide::medals, a release-new DB
// column ABSENT from this tree, so they are deferred -- nothing instantiates the gainer table yet.
// The data + operator& are present so CUnit's save format matches the release byte-for-byte.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMedalsGainer
{
public:
	struct SMedalInfo
	{
		float fPoints;
		int   nProbability;
		bool  bIsGained;
		bool  bIsCollectingPoints;
		bool  bWillBeGiven;
		bool  bJustFound;
		CDBPtr<NDb::CMedal> pMedal;

		SMedalInfo(): fPoints(0), nProbability(0), bIsGained(false),
			bIsCollectingPoints(false), bWillBeGiven(false), bJustFound(false) {}
		int operator&( CStructureSaver &f )
		{
			f.Add(2,&fPoints); f.Add(3,&nProbability); f.Add(4,&bIsGained);
			f.Add(5,&bIsCollectingPoints); f.Add(6,&bWillBeGiven); f.Add(7,&bJustFound);
			f.Add(8,&pMedal); return 0;
		}
	};
	ZDATA
	vector<SMedalInfo> medalInfos;
	CDBPtr<NDb::CRPGPers> pPers;
	bool bDisabled;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&medalInfos); f.Add(3,&pPers); f.Add(4,&bDisabled); return 0; }

	CMedalsGainer(): bDisabled(false) {}

	// --- iMedalsPanel ADDITIVE STUBS (medal-award gather subsystem deferred) -------------------------
	// The release gathers (RPGMedals.obj @0x6acad0 etc.) read each earned medal from side->medals[i],
	// i.e. NDb::CSide::medals -- a release-new DB column ABSENT from this tree (see DataRPG.h CSide:
	// retail tag 11 "is simply not read"). Re-adding that column would shift CSide's byte layout / save
	// tags (forbidden -- the retail game.db is byte-exact), so the two accessors NUI::CMedalsPanel needs
	// are landed here as behaviour-neutral STUBS so that compiland builds GREEN:
	//   GetGainedMedals  -> leaves the output empty (the panel shows no rows; the binary's "no awards"
	//                       state, since no medal can be earned until the DB-schema convergence lands).
	//   HasNewMedalToShow-> false (the panel's perks-tab "new medal" flash is never armed; the binary's
	//                       "no current-award record" path).
	// When NDb::CSide::medals lands, replace these with the faithful gathers (bodies ready in
	// decomp/src/s2_medalsgainer.h). See docs/CONVERGENCE_PROGRESS.md.
	void GetGainedMedals( vector<CDBPtr<NDb::CMedal> >* /*pOut*/ ) {}
	bool HasNewMedalToShow() const { return false; }

	// @0x2ac660 -- credit medal-progress points for one combat/utility event (EMedalPointCases).
	// This is the DIRECT SINK the medal awards funnel into (disarm-trap MPC_DISARM_TRAP @0x3a84f0,
	// CGlobalPlayer::AddMedalPointsForClue, AddMedalPointsForNoticedMines, and the rpgAttackSession
	// tally, all call it as (CMedalsGainer*)(unit+0x38)->AddMedalPoints(game,case,amount)).
	// The release body loops medalInfos, but medalInfos is ALWAYS EMPTY in this tree (the side-driven
	// CMedalsGainer ctor @0x2ac9b0 that fills it needs NDb::CSide::medals, still absent), so every
	// iteration is skipped and the only release-observable effect is two csSystem log lines. Landed as a
	// behaviour-neutral no-op so those award sites link build-safe. Faithful body ready in
	// decomp/src/s2_medalsgainer.h; restore it together with the side-driven ctor + ThrowCheck when
	// NDb::CSide::medals lands. (CMedalsGainer has no vtable -> no layout/save impact.)
	void AddMedalPoints( CGlobalGame* /*pGame*/, EMedalPointCases /*eCase*/, float /*fAmount*/ ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! CUnit holds a unit's stats/skills and the items it carries.
// Release re-architecture: CUnit now derives from CSkilledObject (skills/XP/caps), CNamedObject
// (the name) and CMedalsGainer (medals), in addition to CObjectBase. The dev-era pHead
// (CDBPtr<CComplexHead>) is replaced by the live pHeadInfo (CObj<NLSHead::CHeadInfo>); nRPGPersID is
// no longer cached (read live from pPers via GetRPGPersID()). operator& matches the release
// byte-for-byte (tags 2..0x1b; tag 8 dropped). See docs/CONVERGENCE_PROGRESS.md.
class CUnit : public CObjectBase, public CSkilledObject, public CNamedObject, public CMedalsGainer
{
	OBJECT_BASIC_METHODS(CUnit);
public:
	ZDATA
	wstring wsFullName;                          // release-new full name (tag 4)
	CDBPtr<NDb::CRPGPers> pPers;
	CDBPtr<NDb::CRPGClass> pClass;
	CDBPtr<NDb::CRPGUniform> pUniform;           // release-new (tag 7)
	int nCheats;
	int nHealedVP;
	CObj<NDb::CModel> pModel;                    // release: owning CObj (was CPtr)
	CObj<IInventory> pInventory;
	CPtr<CMeleeWeaponItem> pDefaultWeapon;
	CPtr<CWeaponItem> pCannonItem; // CRAP
private:
	int nDeathVP;
	bool bUnconscious;
	CObj<CPerksTree> pPerksTree;
public:
	CDBPtr<NDb::CRPGPers> pPanzerklein;
	bool bHero;
	CObj<NLSHead::CHeadInfo> pHeadInfo;          // release-new live head (replaces pHead) (tag 0x15)
	CDBPtr<NDb::CString> pBiography;             // release-new (tag 0x16)
	CPtr<IInventoryItem> pAdaptatedWeapon;       // release-new weapon-familiarity (tag 0x17)
	float fAdaptationCounter;                    // release-new (tag 0x18)
	float fCurrentAdaptation;                    // release-new (tag 0x19)
	int nVoice;                                  // release-new (tag 0x1a)
	SRandomSeed bindSeed;                        // release-new bind-places seed (tag 0x1b)
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,(CSkilledObject*)this);
		f.Add(3,(CNamedObject*)this);
		f.Add(4,&wsFullName);
		f.Add(5,&pPers);
		f.Add(6,&pClass);
		f.Add(7,&pUniform);
		f.Add(9,&nCheats);
		f.Add(10,&nHealedVP);
		f.Add(11,&pModel);
		f.Add(12,&pInventory);
		f.Add(13,&pDefaultWeapon);
		f.Add(14,&pCannonItem);
		f.Add(15,&nDeathVP);
		f.Add(16,&bUnconscious);
		f.Add(17,&pPerksTree);
		f.Add(18,&pPanzerklein);
		f.Add(19,&bHero);
		f.Add(20,(CMedalsGainer*)this);
		f.Add(21,&pHeadInfo);
		f.Add(22,&pBiography);
		f.Add(23,&pAdaptatedWeapon);
		f.Add(24,&fAdaptationCounter);
		f.Add(25,&fCurrentAdaptation);
		f.Add(26,&nVoice);
		f.Add(27,&bindSeed);
		return 0;
	}
	//
	CUnit();
	// pInHandItem/pBackpack: the map builder's rolled loot (retail CUnit ctor @0x2bc980 params 7/8,
	// from SMapUnit +108/+112) -- when set they REPLACE the persona's default hand weapon / item
	// list; null keeps the pers defaults (the only path before the chest-loot subsystem).
	CUnit( NDb::CRPGPers *pPers, NDb::CComplexHead *pHead = 0, bool _bHero = false, NDb::CModel *pOverrideModel = 0,
		NDb::CRPGItem *pInHandItem = 0, NDb::CRPGChestReal *pBackpack = 0 );

	void AddXP( float nXPToAdd );
	bool UseSkill( const int eSkill, const int nAddValue );
	int GetSkillBaseStatValue( const int eSkill );

	NDb::CRPGPers* GetPers() const;
	NDb::CComplexHead* GetHead() const;          // the head TEMPLATE (resolved via pHeadInfo)
	NLSHead::CHeadInfo* GetHeadInfo() const { return pHeadInfo; }
	SRandomSeed GetHeadSeed() const;             // per-unit head-randomization seed (retail CreateLSHead source)
	int GetRPGPersID() const;                    // live read of pPers->nRPGPersID
	// retail GetAckHolder @0x2ba8f0 (pers-id form): the pers whose ACK rows this unit's barks use.
	// A HERO maps per-VOICE to the donor pers of its voice family (same Side + gender, nVoice
	// match); everyone else -- and a hero with no donor -- uses the own pers id.
	int GetAckPersID() const;

	void SetHead( NDb::CComplexHead *pNewHead );  // build pHeadInfo from a CComplexHead template
	void SetHeadInfo( NLSHead::CHeadInfo *pNewHead ); // install a live head directly (@0x192070; advanced FaceGen)
	void SetVoice( int _nVoice );                 // assign voice id (+3 for a female persona)
	int  GetVoice() const { return nVoice; }

	NDb::EWeaponType GetWeaponType() const;
	NDb::CAnimWeaponType* GetDBAnimWeapon() const;
	NDb::ESkillType  GetWeaponSkill() const;
	int GetWeaponAP( CWeaponItem *_pWeapon = 0 ) const;
	int GetWeaponBurstAP( CWeaponItem *_pWeapon = 0 ) const;
	int GetWeaponReloadAP( CWeaponItem *_pWeapon = 0 ) const;
	IInventory *GetInventory() { return pInventory; }
	void SetCannonItem( CWeaponItem *pItem ) { pCannonItem = pItem; }
	CWeaponItem* GetCannonItem() { if ( !IsValid(pCannonItem) ) pCannonItem = 0; return pCannonItem; }
	//
	CWeaponItem* GetWeaponItem() const;
	CMeleeWeaponItem* GetMeleeWeaponItem() const;
	NRPG::IFirstAidItem* GetFirstAidItem() const;

	int GetSkillCap( NDb::ESkillType eSkill, float fXP );
	float GetXPForSkill( NDb::ESkillType eSkill, int nLvl );
	bool IsDead();
	void Kill();
	void SetXPLevel( int nLevel );
	bool IsCheatEnabled( int nCheat );
	void SetCheat( int nCheat, bool bState );
	void UpdateSkills();
	int GetDeathVP() { return nDeathVP; }
	void CalcDeathVP( float _fDeathCoeff );
	//
	void CreateFirstAid( SFirstAid *pRes, int nHealVP, int nSkill ) const;
	bool CreateFirstAid( SFirstAid *pRes, int nMaxSpentAP, float fKitCapacity,
		IFirstAidItem *pItem, CUnit *pTarget, int *pRequiredAP );
	int GetFirstAidDC( IFirstAidItem *pItem );

	void Heal( const SFirstAid &fa );
	void RegenerateVP( const SFirstAid &fa );
	bool CanHeal( CUnit *pTarget ) const;
	const bool IsUnconscious() const { return bUnconscious; }
	void SetUnconscious( bool _bUnconscious ) { bUnconscious = _bUnconscious; }
	CPerksTree* GetPerksTree() const { return pPerksTree; }
	NDb::CString* GetBiography() const { return pBiography; }
	bool HasPerk( int nPerkID, float *pParam1 = 0, float *pParam2 = 0, float *pParam3 = 0 ) const;
	float GetWeaponAdaptation( IInventoryItem *pItem ) const;   // retail @0x2bb4a0: live familiarity of pItem (0 unless adapted to it)
	// retail @0x2bba60: per-shot familiarity update. Same weapon: counter=Min(nMaxAdaptation,counter+fRate),
	// bonus=fRate*counter. Other weapon: counter-=fRate; drained (<=0) -> switch pAdaptatedWeapon to pItem,
	// counter=0; bonus=fOtherRate*counter. Called from CreateAttack's adaptation tail.
	void UseWeapon( IInventoryItem *pItem, float fRate, int nMaxAdaptation, float fOtherRate );
	bool IsHero() const { return bHero; }
	// retail CUnit::GetSightDistance @0x2ba680: a FLAT 20.0 world units, scaled by perk 0x53's param when
	// present. (The per-pose CUnitMission::GetSightDistance table is a different, older surface -- retail's
	// AI cover/vision planning reads THIS one via CGame::GetUnitSightDistance @0x2984d0.)
	float GetSightDistance() const
	{
		float fParam = 1.0f;
		if ( HasPerk( 0x53, &fParam ) )
			return fParam * 20.0f;   // 0x41a00000
		return 20.0f;
	}
	// retail CUnit::GetSightFOV @0x2ba6c0: PI radians (a 180-degree total field of view), scaled by perk
	// 0x52's param when present.
	float GetSightFOV() const
	{
		float fParam = 1.0f;
		if ( HasPerk( 0x52, &fParam ) )
			return fParam * 3.1415927f;   // FP_PI
		return 3.1415927f;
	}
};
int GetSkillByCap( int nCap, float fXP );
float GetXPBySkill( int nCap, int nLvl );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
