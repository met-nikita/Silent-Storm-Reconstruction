#include "StdAfx.h"
#include "RPGUnit.h"
#include "LSHead.h"        // NLSHead::CHeadInfo complete type (SetHead / GetHead)
#include "RPGItemSet.h"
#include "A5Script.h"
#include "..\Misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataChest.h"	// NDb::CRPGChestReal / SLootItem (ctor pBackpack consumption)
#include "rpgPerk.h"
#include "rpgPerkConstants.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
const int N_DEFAULT_WEAPON_ID = 1;  // MeleeWeapon - "Unarmed"
const float LN161 = 0.476234179f;
int GetSkillByCap( int nCap, float fXP )
{
	if ( fXP <= 0 )
		return 0;
	const float C = 1.f / ( 20.f * LN161 / float(nCap) );
	return log( 1.f + 0.02f * fXP ) * C;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetXPBySkill( int nCap, int nLvl )
{
	if ( nLvl <= 0 )
		return 0;
	const float C = 1.f / ( 20.f * LN161 / float(nCap) );
	return ( pow( 2.7296f, float(nLvl) / C ) - 1.f ) / 0.02f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicSkill (retail modifier-list model; RPGUnit.obj)
////////////////////////////////////////////////////////////////////////////////////////////////////
// x87 round-to-nearest for the skill math (retail Update/Upgrade fistp under the default CW).
static inline int SkillRound( float f ) { return int( f + ( f < 0 ? -0.5f : 0.5f ) ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2bbb20: recompute the cap from base+XP and every live modifier, sweeping dead
// (released) modifier entries out of the weak list, then clamp the live value DOWN to the cap.
void CDynamicSkill::Update()
{
	nMaxValue = nXPValue + nBaseValue;
	float fMul = 1.0f;
	float fAdd = 0.0f;
	for ( int i = 0; i < modifiers.size(); )
	{
		CSkillModifier *p = modifiers[i];
		if ( !IsValid( p ) )
		{
			modifiers.erase( modifiers.begin() + i );  // compact, do not advance
			continue;
		}
		fMul *= p->fMul;
		fAdd += p->fAdd;
		++i;
	}
	nMaxValue = SkillRound( float( nMaxValue ) * fMul + fAdd );
	if ( nMaxValue <= nValue )
		nValue = nMaxValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2bc6a0: attach a modifier; its flat bonus is applied to the live value immediately
// (TRUNCATING cast, unlike Update's round-to-nearest -- faithful to the two fistp control words).
void CDynamicSkill::AddModifier( CSkillModifier *p )
{
	modifiers.push_back( p );
	if ( p && p->fAdd > 0.0f )
		nValue = int( float( nValue ) + p->fAdd );
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2bbbe0: detach by pointer identity; not found -> no-op (and no Update).
void CDynamicSkill::RemoveModifier( CSkillModifier *p )
{
	for ( int i = 0; i < modifiers.size(); ++i )
	{
		if ( modifiers[i].GetPtr() == p )
		{
			modifiers.erase( modifiers.begin() + i );
			Update();
			return;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// dev-compat setters re-expressed over the retail members (retail inlines these flows): shift
// the XP part so the derived cap lands on the requested figure, keeping the live value in step.
void CDynamicSkill::SetNewMaxValue( int nNewValue )
{
	nXPValue += nNewValue - nMaxValue;
	nValue += nNewValue - nMaxValue;
	nMaxValue = nNewValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CSkilledObject::SetNewBaseValue @0x2bbef0 shape: re-sync the live value off the new
// base directly, then recompute the cap.
void CDynamicSkill::SetNewBaseValue( int nNewValue )
{
	nValue += nNewValue - nBaseValue;
	nBaseValue = nNewValue;
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2bd1a0: a clamped value-only shift (the Jan03 version bumped BOTH nValue and
// nMaxValue with no clamp; retail reworked it).
void CDynamicSkill::Modify( int nModif )
{
	int nBase = nMaxValue <= nValue ? nMaxValue : nValue;
	int nSum = nBase + nModif;
	nValue = nSum < nMaxValue ? nSum : nMaxValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2bbc30: accumulate progress toward the XP cap; whole points move into nXPValue
// (clamped to ROUND(fCap)) and the cap is recomputed via Update().
bool CDynamicSkill::Upgrade( float fAddToProgress, float fCap )
{
	if ( float( nXPValue ) + fProgress < fCap )
	{
		fProgress += fAddToProgress;
		int nModif = SkillRound( fProgress );
		if ( nModif > 0 )
		{
			nXPValue += nModif;
			fProgress -= float( nModif );
			int nCap = SkillRound( fCap );
			if ( nXPValue > nCap )
				nXPValue = nCap;
			Update();
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CWorld::CreateAIUnits @0x36b2b0 (inline): cap = ROUND(current * coeff); live value
// clamped to the new cap (a direct write past Update -- faithful to the retail flow).
void CDynamicSkill::ScaleForDifficulty( float fCoeff )
{
	int nScaled = SkillRound( float( int( *this ) ) * fCoeff );
	nMaxValue = nScaled;
	if ( nValue < nScaled )
		nScaled = nValue;
	nValue = nScaled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkillModifier -- retail @0x295e40 / @0x295ee0: RAII install/detach on the target cell.
////////////////////////////////////////////////////////////////////////////////////////////////////
CSkillModifier::CSkillModifier( CDynamicSkill *_pSkill, const SSkillModifyInfo &info )
	: pSkill( _pSkill ), fMul( info.fMul ), fAdd( info.fAdd )
{
	if ( IsValid( pSkill ) )
		pSkill->AddModifier( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSkillModifier::~CSkillModifier()
{
	if ( IsValid( pSkill ) )
		pSkill->RemoveModifier( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit::CUnit(): nHealedVP( 0 ), nCheats( 0 ), nDeathVP( 0 ), bUnconscious( false ), bHero( false ),
	fAdaptationCounter( 0 ), fCurrentAdaptation( 0 ), nVoice( 0 )
{
	fXP = 0;                                       // CSkilledObject base member
	skills.resize(NDb::SKILL_TYPE_NUMBERS);
	cap.resize(NDb::SKILL_TYPE_NUMBERS, 0);        // CSkilledObject::cap (release-new)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetSkillCap( NDb::ESkillType eSkill, float fXP )
{
	return GetSkillByCap( pClass->skills[eSkill], fXP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnit::GetXPForSkill( NDb::ESkillType eSkill, int nLvl )
{
	if ( !pClass )
		return 0;
	return GetXPBySkill( pClass->skills[eSkill], nLvl );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit::CUnit( NDb::CRPGPers *_pPers, NDb::CComplexHead *_pHead, bool _bHero, NDb::CModel *_pOverrideModel,
	NDb::CRPGItem *_pInHandItem, NDb::CRPGChestReal *_pBackpack ):
	pPers( _pPers ), nHealedVP( 0 ), nCheats( 0 ),
	nDeathVP( 0 ), bUnconscious( false ), bHero( _bHero ),
	fAdaptationCounter( 0 ), fCurrentAdaptation( 0 ), nVoice( 0 )
{
	fXP = 0;                                       // CSkilledObject base member
	skills.resize( NDb::SKILL_TYPE_NUMBERS );
	cap.resize( NDb::SKILL_TYPE_NUMBERS, 0 );      // CSkilledObject::cap (release-new)
	wsName = L"[UNKNOWN]";                          // CNamedObject base member

	// head: explicit template, else the persona's default head -> build the live pHeadInfo
	NDb::CComplexHead *pUseHead = _pHead;
	if ( !IsValid( pUseHead ) )
		pUseHead = pPers->pHead;
	SetHead( pUseHead );

	if ( IsValid( pPers->pName ) )
		wsName = pPers->pName->szStr;

	// pPers->pClass can be NULL -- e.g. the 887/888 "Male"/"Female" gender-preview personas that the
	// CharGen screen previews before a class is chosen. Retail CUnit::CUnit (@0x2bc980) guards the
	// class-derived init: CreatePerksTree (decomp `if (pClass != 0) CreatePerksTree(...)`) and the
	// per-skill cap fill run ONLY for a classed persona. The dev dereferenced pClass unconditionally,
	// crashing (NULL+0x10 read) the moment a gender radio built the preview merc.
	pPerksTree = IsValid( pPers->pClass ) ? CreatePerksTree( pPers->pClass->nPerkTreeID ) : 0;
	pInventory = CreateInventory(this);
	NDb::CRPGBaseValue &baseValues = *pPers->pBaseValue;
	skills[NDb::ST_STR] = new CDynamicSkill( baseValues.skills[NDb::ST_STR], baseValues.skills[NDb::ST_STR] );
	skills[NDb::ST_DEX] = new CDynamicSkill( baseValues.skills[NDb::ST_DEX], baseValues.skills[NDb::ST_DEX] );
	skills[NDb::ST_INT] = new CDynamicSkill( baseValues.skills[NDb::ST_INT], baseValues.skills[NDb::ST_INT] );
	fXP = baseValues.nBaseXP;

	pClass = pPers->pClass;
	pUniform = pPers->pUniform;                    // release-new (CUnit::pUniform)
	pBiography = pPers->pBiography;                // retail CUnit::CUnit @0x2bc980: seed the unit's bio from the persona record
	if ( IsValid( pClass ) )                        // cap stays 0 (resize default) for a classless persona
		for ( int iCap = 0; iCap < NDb::SKILL_TYPE_NUMBERS; ++iCap )
			cap[iCap] = pClass->skills[iCap];      // CSkilledObject::cap from the class caps
	// retail CUnit::CUnit @0x2bc980: the merc's voice is the GENDER-DEFAULT, not pPers->nVoice -- the ctor sets
	// nVoice=0 then +3 for a female persona (i.e. SetVoice(0) == 0 male / 3 female), and never reads pPers->nVoice
	// (@0xb0/176). The old SetVoice(pPers->nVoice) DOUBLE-counted the female +3 for any persona whose DB "Voice"
	// column is already an absolute female index (3-5): e.g. a female preset with Voice=4 -> 4+3=7, out of the
	// valid 0-5 range, so dialogue's unguarded voices[GetVoice()] (iMissionDlgUI.cpp) read past the array -> the
	// random "wrong line each game start". FaceGen's voice buttons still pass a RELATIVE 0-2 into SetVoice (->3-5
	// female), which is the correct relative->absolute mapping; only this creation-time default was wrong.
	SetVoice( 0 );

	// release model-override (CharGen model_left/right): use the pre-rolled override body model when
	// supplied, otherwise roll a fresh one from the persona's CTRndModel as before.
	if ( IsValid( _pOverrideModel ) )
		pModel = _pOverrideModel;
	else
	{
		SRand rand;
		pModel = pPers->pModel->CreateModel( &rand );
	}

	if ( IsValid( pPers->pPanzerklein ) ) 
		pInventory->SetPanzerklein( pPers->pPanzerklein, 0 );
	pDefaultWeapon = dynamic_cast<CMeleeWeaponItem*>( CreateMeleeWeaponItem( NDb::GetMeleeWeapon( N_DEFAULT_WEAPON_ID ) ) );

	//pInventory->Equip( NDb::SLOT_WEAPON, CreateWeaponItem( pPers->pWeapon ) );
	vector< CPtr<IInventoryItem> > items;
	CPtr<IInventoryItem> pMainWeapon;
	// retail @0x2bc980 param 7: a rolled in-hand item (from the pers' WeaponInHand chest, via
	// SMapUnit::pInHandItem) REPLACES the pers' default hand weapon; the pers default is the
	// fallback only when no rolled item (or one without a successor) was supplied.
	if ( IsValid( _pInHandItem ) && IsValid( _pInHandItem->pSuccessor ) )
	{
		pMainWeapon = CreateItem( _pInHandItem->pSuccessor );
		if ( IsValid( pMainWeapon ) )
			pInventory->Equip( NDb::SLOT_1, pMainWeapon );
	}
	else if ( IsValid( pPers->pWeapon ) )
	{
		pMainWeapon = CreateWeaponItem( pPers->pWeapon );
		if ( IsValid( pMainWeapon ) )
			pInventory->Equip( NDb::SLOT_1, pMainWeapon );
	}
	if ( IsValid( pPers->pPanzerklein ) )
	{
		NDb::CPanzerklein *pPK = pPers->pPanzerklein;
		CPtr<IInventoryItem> pSecondWeapon;
	//	pSecondWeapon = CInventoryItem( pPK->pLeftHandItem );
		if ( IsValid( pSecondWeapon ) )
			pInventory->Equip( NDb::SLOT_2, pSecondWeapon );			
	}
	// retail @0x2bc980 param 8: a rolled backpack chest (from the pers' WeaponInBackpack chest +
	// clips migrated off the hand chest, via SMapUnit::pBackpack) REPLACES the pers' default item
	// list; the pers list fills the backpack only when no chest was rolled.
	if ( IsValid( _pBackpack ) )
	{
		for ( vector<NDb::SLootItem>::const_iterator it = _pBackpack->items.begin(); it != _pBackpack->items.end(); ++it )
		{
			if ( !IsValid( it->pItem ) || !IsValid( it->pItem->pSuccessor ) )
				continue;
			for ( int i = 0; i < it->nQuantity; ++i )
			{
				CPtr<NRPG::IInventoryItem> pItem = CreateItem( it->pItem->pSuccessor );
				if ( IsValid( pItem ) )
					pInventory->Place( CTPoint<int>( -1, -1 ), pItem );
			}
		}
	}
	else
	for ( vector<NDb::SItemAssign>::const_iterator it = pPers->items.begin(); it != pPers->items.end(); ++it )
	{
		const NDb::SItemAssign &assign = *it;
		for( int i = 0; i < assign.nQuantity; ++i )
		{
			if ( IsValid( assign.pItem ) )
			{
				CPtr<NRPG::IInventoryItem> pItem;
				if (!IsValid(assign.pAmmo))
					pItem = CreateItem(assign.pItem);
				else
				{
					CDynamicCast<NDb::CRPGClip> pRPGClip(assign.pItem);
					if (pRPGClip)
						pItem = CreateClipItem(pRPGClip, assign.pAmmo);
				}

				if ( IsValid( pItem ) )
					pInventory->Place( CTPoint<int>( -1, -1 ), pItem );
			}
			else
				ASSERT( 0 && "wrong item was assigned to unit" );
		}
	}

	for ( int i = 0; i < NDb::ST_STR; ++i )
		skills[i] = new CDynamicSkill( baseValues.skills.skills[i], GetSkillBaseStatValue( NDb::ESkillType(i) ) );
	if ( pClass )
	{
		for ( int i = NDb::ST_MELEE; i < NDb::ST_STR; ++i )
		{
			if ( baseValues.skills.skills[i] == 0 )
			{
				Skills(i).SetXPPart( GetSkillCap( NDb::ESkillType(i), fXP ) );
				Skills(i).Reset();
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetName/SetName now live on the CNamedObject base (inline). GetRPGPersID reads the persona
// record live (the cached nRPGPersID member was dropped in the release re-architecture).
int CUnit::GetRPGPersID() const
{
	if ( IsValid( pPers ) )
		return pPers->nRPGPersID;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::CUnit::GetAckHolder @0x2ba8f0, pers-id form -- DISASM-DECODED (capstone @0x6ba8f0):
//   hero (bHero @+0x8c):  pers = pPers(+0x58); side = pers->pSide(+0x7c);
//                      walk side->defaultPersesSet (the vector at CSide+0x40) and return the FIRST
//                      entry whose nVoice(+0xb0) == this->nVoice(+0xa4); no match -> NULL holder.
//                      (The default sets carry ONE donor per voice: side 1 = AxisGrenader v0 /
//                      AxisSoldier v1 / AxisEngineer v2 / AxisMedicFemale v4 / ... -- voice-unique,
//                      so the match is deterministic. The previous dev scan over the WHOLE pers
//                      table by (voice,gender,side) hit an arbitrary hash-order persona: for voice 0
//                      there are ~49 same-key personas and only ~17 own ack rows, so it usually
//                      resolved to a bark-less pers.)
//   non-hero:          return pPers->pAcksHolder (+0xd0, the "AckUnit" column) -- in the retail
//                      game.db 499/728 personas (every generic enemy/ally/NPC, e.g. all Germ_*
//                      soldiers -> pers 911) key their ack rows ONLY via that link.
//   Callers (retail AddAck @0x3396b0 / GetPersAck @0x2452c0): NULL/dead holder -> own GetRPGPersID().
int CUnit::GetAckPersID() const
{
	if ( IsValid( pPers ) )
	{
		if ( bHero )
		{
			NDb::CSide *pSide = pPers->pSide;	// plain extraction (no ternary over CPtr -- UAF)
			if ( IsValid( pSide ) )
			{
				for ( int k = 0; k < pSide->defaultPersesSet.size(); ++k )
				{
					NDb::CRPGPers *p = pSide->defaultPersesSet[k];
					if ( IsValid( p ) && p->nVoice == nVoice )
						return p->nRPGPersID;
				}
			}
		}
		else
		{
			NDb::CRPGPers *pHolder = pPers->pAcksHolder;	// plain extraction (no ternary over CPtr -- UAF)
			if ( IsValid( pHolder ) )
				return pHolder->nRPGPersID;
		}
	}
	return GetRPGPersID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetHead @0x2bb970 -- replace the unit's live head info from a CComplexHead template, seeding the
// fresh CHeadInfo deterministically from this unit's address (per-unit head randomization).
void CUnit::SetHead( NDb::CComplexHead *pNewHead )
{
	pHeadInfo = 0;                                 // release the current head (CObj assign to null)
	if ( !pNewHead )
		return;
	pHeadInfo = new NLSHead::CHeadInfo( pNewHead );
	pHeadInfo->SetSeed( SRandomSeed( (int)this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetHeadInfo @0x192070 -- install an already-built live head directly (the CObj assignment open-codes the
// intrusive AddRef-new/Release-old). Used by the advanced FaceGen editor to commit the morphed head on "play"
// and to restore the original on "cancel".
void CUnit::SetHeadInfo( NLSHead::CHeadInfo *pNewHead )
{
	pHeadInfo = pNewHead;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetHeadSeed -- the per-unit head-randomization seed (SetHead seeds it from this unit's address). Retail
// CGameView::CreateLSHead (@0x188c90) seeds the head hair/material rnd from this CHeadInfo::seed, so each
// unit head is stable (non-cycling) yet per-unit distinct; null-safe for a headless unit.
SRandomSeed CUnit::GetHeadSeed() const
{
	return IsValid( pHeadInfo ) ? pHeadInfo->GetSeed() : SRandomSeed();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetVoice @0x2bb1f0 -- assign the unit's voice id, bumping it by 3 for a female persona.
void CUnit::SetVoice( int _nVoice )
{
	nVoice = _nVoice;
	if ( IsValid( pPers ) && pPers->bIsFemale )
		nVoice = _nVoice + 3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::UpdateSkills()
{
	for ( int i = 0; i < NDb::ST_STR; ++i )
	{
		skills[i]->SetNewBaseValue( GetSkillBaseStatValue( NDb::ESkillType(i) ) );
		skills[i]->Reset();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::AddXP( float fXPToAdd )
{
	if ( !IsValid( pPers ) || !IsValid( pPers->pName ) )
		return;
	//
	int nOldLevel = Skills( NDb::ST_LEVEL );
	csRPG << CC_YELLOW << pPers->pName->szStr << " gained " << fXPToAdd << " EXP" << endl;
	fXP += fXPToAdd;
	// FIX (2026-06-25): advance the level VALUE while the total XP reaches the next level's threshold. The
	// predecessor only set the level skill's PROGRESS bar (SetProgress) and never advanced its value, so units
	// never actually levelled up -- nNewLevel-nOldLevel was always 0, so the perk points a level grants (and any
	// other level-up effect) were never awarded. Retail CSkilledObject::AddXP @0x6bbde0 loops the level-up the
	// same way; GetXPBySkill is exponential (strictly increasing) so the loop terminates (the nGuard bound is
	// belt-and-suspenders against a degenerate XP-for-level table).
	for ( int nGuard = 0; nGuard < 1000 && fXP >= GetXPForSkill( NDb::ST_LEVEL, Skills( NDb::ST_LEVEL ) + 1 ); ++nGuard )
		Skills( NDb::ST_LEVEL ).SetValue( Skills( NDb::ST_LEVEL ) + 1 );
	// the progress bar within the (now current) level
	int nLvlEXP = GetXPForSkill( NDb::ST_LEVEL, Skills( NDb::ST_LEVEL ) );
	int nLvlUPEXP = GetXPForSkill( NDb::ST_LEVEL, Skills( NDb::ST_LEVEL ) + 1 );
	if ( nLvlUPEXP > nLvlEXP )
		Skills( NDb::ST_LEVEL ).SetProgress( float( fXP - nLvlEXP ) / float( nLvlUPEXP - nLvlEXP ) );
	//
	int nNewLevel = Skills( NDb::ST_LEVEL );
	if ( nNewLevel > nOldLevel )
	{
		if ( IsValid( pPerksTree ) )
			pPerksTree->AddPerkPoints( nNewLevel - nOldLevel );
		csRPG << CC_YELLOW << pPers->pName->szStr << " level up ( " << Skills( NDb::ST_LEVEL ) << "-th level )" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::UseSkill( int eSkill, const int nAddValue )
{
	CDynamicSkill &skill = Skills(eSkill);
	float fCap = GetSkillCap( NDb::ESkillType(eSkill), fXP );
	float sSkillDiff = fCap - ( skill.GetXPPart() - nAddValue );
	float fAdd = Clamp( (1.f - skill.GetProgress()) * sSkillDiff / 20.f, 0.005f, 0.1f );
	// retail CSkilledObject::UseSkill @0x2bbf40 passes the XP cap into Upgrade (the amount
	// formula there divides by the GetRPGBaseValue(0x3f) per-skill rate -- follow-up).
	bool bRes = skill.Upgrade( fAdd, fCap );
	if ( eSkill >= NDb::ST_STR )
		UpdateSkills();
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetSkillBaseStatValue( const int eSkill )
{
	switch ( eSkill )
	{
		case NDb::ST_MELEE:
			return Skills(NDb::ST_STR) + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_SHOOTING:
			return 20 + Skills(NDb::ST_DEX);
		case NDb::ST_THROWING:
			return 2 * Skills(NDb::ST_STR) + Skills(NDb::ST_DEX);
		case NDb::ST_BURST:
			return 2 * Skills(NDb::ST_STR) + Skills(NDb::ST_DEX);
		case NDb::ST_SNIPE:
			return 2 * Skills(NDb::ST_DEX) + Skills(NDb::ST_INT);
		//
		case NDb::ST_STEALTH:
			return 5 + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_SPOT:
			return 5 + 2 * Skills(NDb::ST_INT);
		case NDb::ST_MEDICINE:
			return Skills(NDb::ST_DEX) + 2 * Skills(NDb::ST_INT);
		case NDb::ST_ENGINEERING:
			return Skills(NDb::ST_DEX) + 2 * Skills(NDb::ST_INT);
		//
		case NDb::ST_VP:
			return 50 + 10 * Skills(NDb::ST_STR);
		case NDb::ST_INTERRUPT:
			return 25; //CRAP
		case NDb::ST_AP:
			return 40 + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_IC:
			return 5;
		case NDb::ST_LEVEL:
			return 0;
		default:
			ASSERT( 0 && "Unknow skill" );
			break;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* CUnit::GetPers() const
{
	return pPers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CComplexHead* CUnit::GetHead() const
{
	if ( IsValid( pHeadInfo ) )
		return pHeadInfo->GetHead();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CUnit::GetWeaponType() const
{
	CDynamicCast<NRPG::CMineDetectorItem> pMD(pInventory->GetActive());
	if (pMD)
		return NDb::WT_MINE_DETECTOR;

	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetWeaponType();

	CMeleeWeaponItem *pMeleeWeapon = GetMeleeWeaponItem();
	if ( pMeleeWeapon )
		return pMeleeWeapon->GetWeaponType();

	return NDb::WT_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CAnimWeaponType* CUnit::GetDBAnimWeapon() const
{
	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetDBWeapon()->pAnimWeaponType;
	CMeleeWeaponItem *pMeleeWeapon = GetMeleeWeaponItem();
	if ( pMeleeWeapon )
		return pMeleeWeapon->GetDBMeleeWeapon()->pAnimWeaponType;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::ESkillType CUnit::GetWeaponSkill() const
{
	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetSkillIndex();
	return NDb::ST_MELEE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponAP( CWeaponItem *_pWeapon ) const
{
	int nRes = 0;
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		nRes = pWeapon->GetShootAP();
		NDb::EShootMode ShotMode = pWeapon->GetShootMode();
		if ( ShotMode == NDb::SM_Aimed || ShotMode == NDb::SM_Careful )
			nRes += pWeapon->GetDBWeapon()->nTargetingAP;		
		//
		float fDelta = 0;
		if ( ShotMode == NDb::SM_Snap )
			HasPerk( N_PERK_CHEAP_SNAP_SHOT, &fDelta );
		else if ( ShotMode == NDb::SM_Aimed )
			HasPerk( N_PERK_CHEAP_AIMED_SHOT, &fDelta );
		else if ( ShotMode == NDb::SM_ShortBurst )
			HasPerk( N_PERK_CHEAP_SHORT_BURST, &fDelta );
		nRes -= nRes * fDelta;
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponBurstAP( CWeaponItem *_pWeapon ) const
{
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		if ( 0 == pWeapon->GetDBWeapon()->nRoF )
		{
			ASSERT( 0 );
			return 0;
		}
		return skills[NDb::ST_AP]->GetMaxValue() / pWeapon->GetDBWeapon()->nRoF;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponReloadAP( CWeaponItem *_pWeapon ) const
{
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
		return pWeapon->GetReloadAP();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWeaponItem* CUnit::GetWeaponItem() const
{
	if ( pCannonItem )
		return pCannonItem;
	CDynamicCast<NRPG::CWeaponItem> pWeapon(pInventory->GetActive());
	if (pWeapon)
		return pWeapon;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMeleeWeaponItem* CUnit::GetMeleeWeaponItem() const
{
	IInventoryItem *pItem = pInventory->GetActive();
	if ( !pItem )
		return pDefaultWeapon;
	CDynamicCast<NRPG::CMeleeWeaponItem> pWeapon(pItem);
	if (pWeapon)
		return pWeapon;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::IsDead()
{
	return Skills( NDb::ST_VP ) <= nDeathVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::Kill()
{
	Skills(NDb::ST_VP ).SetValue( -0xFFF );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::CalcDeathVP( float _fDeathCoeff )
{
	nDeathVP = -_fDeathCoeff * Skills( NDb::ST_VP ).GetMaxValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::SetXPLevel( int nLevel )
{
	if ( !pClass )
		return;
	int nOldLevel = Skills( NDb::ST_LEVEL );
	float fXP = GetXPForSkill( NDb::ST_LEVEL, nLevel );
	for ( int i = NDb::ST_MELEE; i < NDb::SKILL_TYPE_NUMBERS; ++i )
	{
		Skills(i).SetXPPart( GetSkillCap( NDb::ESkillType( i ), fXP ) );
		Skills(i).Reset();
	}
	UpdateSkills();
	csRPG << CC_GREY << "Unit [ " << CC_YELLOW << GetName() << 
		CC_GREY << " ] level changed: " << Skills( NDb::ST_LEVEL ) << "-th level" << endl;
	int nNewLevel = Skills( NDb::ST_LEVEL );
	if ( IsValid( pPerksTree ) )
		pPerksTree->AddPerkPoints( nNewLevel - nOldLevel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::IsCheatEnabled( int nCheat )
{
	return ( ( nCheats & nCheat ) > 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::SetCheat( int nCheat, bool bState )
{
	if ( bState )
		nCheats |= nCheat;
	else
		nCheats &= ( ~nCheat );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IFirstAidItem* CUnit::GetFirstAidItem() const
{
	NRPG::IInventoryItem *pItem = pInventory->GetActive();
	if ( pItem == 0 )
		return 0;
	return CDynamicCast<NRPG::IFirstAidItem>( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float fVPScale = (float)N_MAX_VP / N_MAX_SKILL;
const float fDCScale = (float)N_MAX_DC / N_MAX_SKILL;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::CreateFirstAid( SFirstAid *pRes, int nHealVP, int nSkill ) const
{
	pRes->nMaxVP = fVPScale * nSkill;
	pRes->fdVP = nHealVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::CreateFirstAid( SFirstAid *pRes, int nMaxSpentAP, float fKitCapacity, 
	IFirstAidItem *pItem, CUnit *pTarget, int *pRequiredAP )
{
	SFirstAid tmp;
	int nSkill = *skills[ NDb::ST_MEDICINE ] + pItem->GetDBFirstAid()->nSkillModifier;
	CreateFirstAid( &tmp, 0, nSkill );
	int nMaxHealed = pTarget->Skills(NDb::ST_VP).GetMaxValue() - pTarget->Skills(NDb::ST_VP);
	int nVPToHeal = Max( 0, tmp.nMaxVP - pTarget->nHealedVP );
	nVPToHeal = Min( nVPToHeal, nMaxHealed );
	if ( nVPToHeal == 0 )
		return false;
	int nRequiredAP = 60.f / nSkill * nVPToHeal;
	if ( nRequiredAP > nMaxSpentAP )
	{
		nRequiredAP = nMaxSpentAP;
		nVPToHeal = Float2Int( nRequiredAP * nSkill / 60.0f );
	}
	*pRequiredAP = nRequiredAP;
	CreateFirstAid( pRes, nVPToHeal, nSkill );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetFirstAidDC( IFirstAidItem *pItem )
{
	int nSkill = *skills[ NDb::ST_MEDICINE ];
	if ( IsValid(pItem) ) 
		nSkill += pItem->GetDBFirstAid()->nSkillModifier;
	return fDCScale * nSkill;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::Heal( const SFirstAid &fa )
{
	int nTotalVP = Skills( NDb::ST_VP ) + nHealedVP;
	const int nMaxVP = Min( Skills( NDb::ST_VP ).GetMaxValue(), Skills( NDb::ST_VP ) + fa.nMaxVP );
	const int ndVP = Max( 0, Min( Float2Int( fa.fdVP ), nMaxVP - nTotalVP ) );
	nHealedVP += ndVP;
	//
	csRPG << "<font size=16pt>";
	csRPG << CC_RED << "\tBandage:" << CC_GREY << endl;
	csRPG << "\tMaxVP=" << fa.nMaxVP << " \t dVP=" << Float2Int( fa.fdVP ) << endl;
	csRPG << "<font size=16pt>";
	csRPG << "\t" << GetName() << ": \tVP += " << ndVP << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::RegenerateVP( const SFirstAid &fa )
{
	Skills( NDb::ST_VP ).SetValue( Max( 1, 
		Min( Skills( NDb::ST_VP ).GetMaxValue(), ( int )( Skills( NDb::ST_VP ) + fa.fdVP ) ) ) );
	nHealedVP = Min( nHealedVP, Skills( NDb::ST_VP ).GetMaxValue() - Skills( NDb::ST_VP ) );
	//
	csRPG << "<font size=16pt>";
	csRPG << CC_RED << "\tHeal:" << CC_GREY << endl;
	csRPG << "\t" << GetName() << ": VP = " << Skills( NDb::ST_VP ) << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::CanHeal( CUnit *pTarget ) const
{
	ASSERT( IsValid( pTarget ) );
	if ( !IsValid( pTarget ) )
		return false;
	//
	return int( fVPScale * *skills[ NDb::ST_MEDICINE ] ) > pTarget->nHealedVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::HasPerk( int nPerkID, float *pParam1, float *pParam2, float *pParam3 ) const
{
	return GetPerksTree()->HasPerk( nPerkID, pParam1, pParam2, pParam3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnit::GetWeaponAdaptation @0x2bb4a0: the live weapon-familiarity bonus -- returns the
// current adaptation ONLY while pItem is the very weapon the unit is adapted to (pAdaptatedWeapon),
// else 0. The value is maintained by UseWeapon; this is a pure read for the tooltip's "familiarity".
float CUnit::GetWeaponAdaptation( IInventoryItem *pItem ) const
{
	if ( IsValid( pItem ) && pItem == pAdaptatedWeapon )
		return fCurrentAdaptation;
	return 0.f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnit::UseWeapon @0x2bba60 (disasm-verified): raise familiarity with the weapon in use,
// decay it for any other weapon; a fully drained counter (<= 0) switches the adapted weapon.
void CUnit::UseWeapon( IInventoryItem *pItem, float fRate, int nMaxAdaptation, float fOtherRate )
{
	if ( pItem == pAdaptatedWeapon )
	{
		fAdaptationCounter = Min( (float)nMaxAdaptation, fAdaptationCounter + fRate );
		fCurrentAdaptation = fRate * fAdaptationCounter;
		return;
	}
	fAdaptationCounter -= fRate;
	if ( fAdaptationCounter <= 0 )
	{
		pAdaptatedWeapon = pItem;
		fAdaptationCounter = 0;
	}
	fCurrentAdaptation = fOtherRate * fAdaptationCounter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x24051150, CUnit );
REGISTER_SAVELOAD_CLASS( 0x24051151, CDynamicSkill );
REGISTER_SAVELOAD_CLASS( 0x24051152, CSkillModifier );
