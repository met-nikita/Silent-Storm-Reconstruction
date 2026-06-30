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
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	enum EPose;
}
namespace NLSHead
{
	class CHeadInfo;   // CUnit::pHeadInfo (live head; full type in LSHead.h)
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
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MAX_SKILL = 140;
const int N_MAX_VP = 250;
const int N_MAX_DC = 300;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Skill cell - a dynamic value such as HP or AP
class CDynamicSkill: public CObjectBase
{
	OBJECT_BASIC_METHODS(CDynamicSkill);
private:
	ZDATA
	int nBaseValue; // base value derived from the stat
	int nMaxValue;  // max value the skill can reach for the current XP, plus the base value
	int nValue;     // current value, may be lower than nMaxValue
	float fMultiplier; // current modifier
	float fProgress;   // progress toward the next +1
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBaseValue); f.Add(3,&nMaxValue); f.Add(4,&nValue); f.Add(5,&fMultiplier); f.Add(6,&fProgress); return 0; }
public:
	CDynamicSkill( int nSetValue = 0, int nBaseStatValue = 0 ):
		nBaseValue(nBaseStatValue), nValue(nSetValue), nMaxValue(nSetValue), fMultiplier( 1 ), fProgress( 0 )
	{
		if ( nBaseValue > nMaxValue )
			nMaxValue = nBaseValue;
		fMultiplier = 1.0f;
	}

	void SetNewMaxValue( int nNewValue );
	void SetNewBaseValue( int nNewValue );
	void Modify( int nModif ) { nValue += nModif; nMaxValue += nModif; }
	bool Upgrade( float fAddToProgress );
	int  GetXPPart() const { return nMaxValue - nBaseValue;} // value gained from XP only
	void SetXPPart( int nNewXPPart ) { nValue += nNewXPPart - GetXPPart(); nMaxValue = nBaseValue + nNewXPPart; }
	void Multiply( float fValue );

	float GetProgress() const { return fProgress; }
	int GetMaxValue() const { return nMaxValue; }
	int GetCurrentMaxValue() const { return fMultiplier * nMaxValue; }
	void Reset() { nValue = fMultiplier * nMaxValue; }
	void SetValue( int n ) { nValue = n; }
	void SetProgress( float p ) { fProgress = p; }

	void SetConst( int nConstValue ) { nValue = nMaxValue = nBaseValue = nConstValue; }

	const CDynamicSkill& operator += ( int n ) { nValue += n; if ( nValue > nMaxValue ) nValue = nMaxValue; return *this; }
	const CDynamicSkill& operator -= ( int n ) { nValue -= n; return *this; }
	operator int () const { return nValue; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkillModifier: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CSkillModifier);
	struct SModif
	{
		CPtr<CDynamicSkill> pSkill;
		float fMultiplier; // applied modifier

		SModif(): fMultiplier(1) {}
		SModif( CDynamicSkill *_pSkill, float fModif ) : pSkill(_pSkill), fMultiplier(fModif) { pSkill->Multiply(fModif); }
		~SModif() { pSkill->Multiply(1.0f/fMultiplier); }
	}data;
public:
	CSkillModifier() {}
	CSkillModifier( CDynamicSkill *pSkill, float fMultiplier ) : data( pSkill, fMultiplier) {}
	void Set( float fMultiplier )
	{
		data.pSkill->Multiply( 1.0f/data.fMultiplier );
		data.fMultiplier = fMultiplier;
		data.pSkill->Multiply( data.fMultiplier );
	}
	float Get() { return data.fMultiplier; }

	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &data.fMultiplier );
		f.Add( 2, &data.pSkill );
		return 0;
	}
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
	CUnit( NDb::CRPGPers *pPers, NDb::CComplexHead *pHead = 0, bool _bHero = false, NDb::CModel *pOverrideModel = 0 );

	void AddXP( float nXPToAdd );
	bool UseSkill( const int eSkill, const int nAddValue );
	int GetSkillBaseStatValue( const int eSkill );

	NDb::CRPGPers* GetPers() const;
	NDb::CComplexHead* GetHead() const;          // the head TEMPLATE (resolved via pHeadInfo)
	NLSHead::CHeadInfo* GetHeadInfo() const { return pHeadInfo; }
	SRandomSeed GetHeadSeed() const;             // per-unit head-randomization seed (retail CreateLSHead source)
	int GetRPGPersID() const;                    // live read of pPers->nRPGPersID

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
	bool IsHero() const { return bHero; }
};
int GetSkillByCap( int nCap, float fXP );
float GetXPBySkill( int nCap, int nLvl );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
