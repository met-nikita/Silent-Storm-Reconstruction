#include "StdAfx.h"
#include "RPGGlobal.h"
#include "RPGUnitInfo.h"
#include "RPGMerc.h"
#include "RPGItemSet.h"
#include "RPGAttackMech.h"
#include "RPGGame.h"        // NRPG::IGame::GetMaxCriticalSeverity (combat critical clamp)
#include "Grid.h"
#include "A5Script.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAI.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"
#include "aiPosition.h"
#include "RPGCritical.h"
#include "RPGToHit.h"
#include "wUnitServer.h"   // NWorld::CUnitServer / CUnit -- the to-hit dispatch RTTI-casts to the server
#include "wMain.h"
#include "wAckBase.h"
#include "RPGDiplomacy.h"
#include "rpgCheatConstants.h"
#include "rpgPerkConstants.h"
#include "..\DBFormat\DataRpgConstants.h"
#include "..\DBFormat\DataMisc.h"   // NDb::CRPGAP (GetActionAP RPGAP-table costs) + CRPGPicklock (AC_PICK_LOCK nAPToUse)

#include "RPGUnitMission.h"
#include "RPGAttackSession.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
const float F_BACKSTAB_MELEE_COEFF = 2.5f;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCriticalType
{
	ZDATA
	int nStartPos;					// start of the range within which this critical damage is rolled
	CDBPtr<NDb::CRPGCritical> pCritical;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nStartPos); f.Add(3,&pCritical); return 0; }
	SCriticalType( int nStart, NDb::CRPGCritical *p ): nStartPos(nStart), pCritical(p) {}
};
typedef vector<SCriticalType> TCriticalSet;
static vector<TCriticalSet> criticalBar( NDb::N_CL ); //[NDb::N_CL];
static bool bCBarInitialized = false;
int Round( float f ) { return int( f + 0.5f ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// EToHitType moved to RPGUnitMission.h -- the composite tile to-hit / cover entry points
// (RPGGame.cpp) branch on NRPG::GetToHitType like retail RealCalcTileCovers @0x2b4700.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCriticalsHolder
{
	ZDATA
	vector<CObj<CCritical> > criticals;   // retail @0x2c7d80: DoVector<CObj<CCritical>> -- OWNING refs
	int nTimeLeft;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&criticals); f.Add(3,&nTimeLeft); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::SModifierHolder (@0x2c7960 operator&, PDB sizeof 16): a postponed skill modifier --
// after nTimeLeft turns SegmentCriticals rebuilds a CSkillModifier(pTarget, info) and installs it
// into the mission's temporaryModifiers. `info` is a raw 8-byte chunk (tag 3), matching retail.
struct SModifierHolder
{
	ZDATA
	CPtr<CDynamicSkill> pTarget;
	SSkillModifyInfo info;
	int nTimeLeft;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); f.Add(3,&info); f.Add(4,&nTimeLeft); return 0; }
	SModifierHolder(): nTimeLeft( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitMission
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitMission: public CUnitMissionForMedals, public IUnitMission
{
	friend class CUnitToHitCalcer;
	friend class CTileToHitCalcer;
	friend class CGrenadeToHitCalcer;
	friend class CRLauncherToHitCalcer;
	// release migration: the free-fn to-hit dispatch reaches GetToHitWeaponType / GetSnipeAP /
	// GetMeleeToHit / bLogActive (all private) -- befriend it (no visibility change to the class).
	friend int GetToHit( const NWorld::CUnit*, NAI::EPose, int, const CVec3&, const NAI::SPosition&, NAI::EHitLocation, int, const NWorld::CUnit*, const vector<int>&, int, bool, const CVec3&, bool, int, bool );
	friend int GetTileToHit( const NWorld::CUnit*, NAI::EPose, int, const CVec3&, CVec3, NAI::ETileHitLocation, int, int, bool, const CVec3&, int );
	friend int GetGrenadeToHit( const NWorld::CUnit*, NAI::EPose, int, const CVec3&, bool, CVec3, const CVec3& );
	friend int GetRLauncherToHit( const NWorld::CUnit*, NAI::EPose, int, const CVec3&, CVec3, NAI::ETileHitLocation, int, bool, const CVec3& );
	friend EToHitType GetToHitType( const NWorld::CUnit* );

	OBJECT_BASIC_METHODS(CUnitMission);
private:
	void UseSkill( NDb::ESkillType eSkill );
	void ApplyCritical( CCritical *p );
	void ApplyCritical( NDb::CRPGCritical *pCritical, int nDC );
	void ProcessCriticalsOnNewTurnFor();
	float GetCriticalDmgModifier( NWorld::IWorld *pWorld, NAI::EHitLocation eHL,
		int nCriticalProbability, int nCriticalDifficulty, bool bForced, IUnitMissionInfo *pAttacker );
	void CheckOverdose();
	EToHitType GetToHitWeaponType() const;
	int GetMeleeToHit( const CVec3 &ptAttacker, const NAI::SPosition &posTarget, 
		IUnitMissionInfo *pTarget, NAI::EHitLocation hl, const vector<int> &accessibleHLs, bool bBackStab ) const;
	int CalcInterruptProbability( const IUnitMission *pEnemy,	bool bIsMutual, bool bWasShot );
	// retail @0x2bea70: the "unaware target" (backstab) critical scale -- perk 74, crit x(Param1+1)
	// capped at 100, critical difficulty scaled by the same capped factor. Called by both CreateAttack
	// branches with bBackStab.
	void ModifyUnawareCritical( CAttackPortion &a, bool bApply ) const;

	virtual int GetHealedVP() const { return Max( 0, pRPGUnit->nHealedVP ); }
	virtual int GetTotalVP() const { return pRPGUnit->Skills( NDb::ST_VP ) + GetHealedVP(); }
	void SetHealedVP( int n ) { pRPGUnit->nHealedVP = Max( 0, n ); }
	virtual int GetLastActionTimes() const { return nLastActionTimes; }
	virtual int GetMoveInLastTurn() const { return nMoveInLastTurn; }
	virtual float GetWeaponSkill( CObjectBase *pItem, int nExtraAP, int nBullet );
	virtual void AddMoveInLastTurn( int n ) { nMoveInLastTurn += n; }
	virtual int RollCritical( NAI::EHitLocation eHL, int nCriticalDifficulty, NDb::CRPGCritical **pCritical );
	void SaveAck( int nAckID, IUnitMissionInfo *pAttacker );
	void ResetUnitParameters();
	int GetSnipeAP( IUnitMissionInfo *pTarget ) const;
	int GetUnconsciousProbability( IUnitMissionInfo *pAttacker, 
		IUnitMissionInfo *pTarget, int nBaseProbability ) const;
	// retail @0x2bf9c0 (PDB: private CReceivedDmg ProcessAttackForPK(IWorld*,int,CAttackPortion*,
	// CRPGArmor*,bool)) -- pWorld added for CalcStructDmg's difficulty multipliers.
	CReceivedDmg ProcessAttackForPK( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
		NDb::CRPGArmor *pArmor, bool bApplyToVP );

	float GetPanzerkleinAddCoverIgnore() { if ( pPanzerklein ) return pPanzerklein->fAddCoverIgnore; return 0; }
	void WearBrokenPK();
	int GetSkillAddValue( const int eSkill ) const;

	ZDATA
	int nMoveInLastTurn;
	EAction eLastAction;
	int nLastActionTimes;
	vector<CObj<CCritical> > criticals;   // retail tag 5: DoVector<CObj<CCritical>> (owning)
	int _nShameOnEpik;
	bool bLogActive;
	// retail +116 (tag 8, int): Apply/ReleaseMotionless ref-count -- replaces the Jan03 bSitting
	// bool (several motionless sources may overlap). Jan03 nBullet (dev tag 9) and the perception
	// pair (dev tags 14/15) do NOT exist in retail (PDB-confirmed); retail leaves tags 9/14/15
	// as permanent gaps in the table -- reproduced below.
	int  nMotionless;
	SSnipeAP savedSnipeAP;
	float fLastToHit; // for Burst ToHit
	vector<NDb::ECritical> lastCriticals;
	bool bUseTwoHanded;
	NDb::SToHitConstants tohit;
	NDb::SAISoundConstants sAISoundConstants; // AI sound constants
	NDb::SInterruptsConstants SInterruptsConstants; // Interrupt constants
	CVec3 ptLastCP;
	vector<ECriticalState> criticalsState; // affects only the roll of new criticals
	vector<CObj<CSkillModifier> > pkModifiers; // retail +340 (tag 21): worn-Panzerklein skill modifiers (SetPanzerklein @0x2c3b00)
public:
	CPtr<NDb::CModel> pModel;
	CObj<CUnit> pRPGUnit;
	wstring sID;
private:
	int nBulletHitThisTurn; // for criticals
	list< CPtr<IUnitMissionInfo> > AckAttackers;
	list<int> AckIDs;
	SDiplomacy diplomacy;
	bool bUnconscious;
	CDBPtr<NDb::CPanzerklein> pPanzerklein;
	CPtr<CDynamicSkill> pPanzerkleinVP;
	bool bHiding;
	CDBPtr<NDb::CDBMinesConstants> pMinesConstants;
	list<SCriticalsHolder> suspendedCriticals;   // retail tag 34 (the dev pGlobalGame@34 collided with this!)
	int nBleedingStopAmount;                      // retail +412 (tag 35)
	list<SModifierHolder> postponedModifiers;     // retail +416 (tag 36): delayed skill modifiers (SegmentCriticals @0x2c46a0)
	vector<CObj<CSkillModifier> > temporaryModifiers; // retail +420 (tag 37): VP drug boosts etc. (AddVPBoost @0x2c3400)
	vector<int> vpBoostDurations;                 // retail +432 (tag 38): parallel boost lifetimes
	int nTurnCounter;                             // retail +444 (tag 39): ++ per StartNewTurn @0x2bf8c0
	// the mission-side adaptation trio (retail +448..456, tags 40/41/42). The LIVE adaptation state
	// lives on CUnit (tags 23/24/25, landed a883652); retail keeps this serialized mirror on the
	// mission -- retail GetWeaponAdaptation @0x2c0b40 delegates to the CUnit copy, so the mirror is
	// format-only here as in retail.
	CPtr<IInventoryItem> pAdaptatedWeapon;
	float fAdaptationCounter;
	float fCurrentAdaptation;
	float fAuraPerkICModifier;                    // retail +460 (tag 43): SetAuraPerkICModifier @0x2c5b90
	int nScenarioPlayerID;                        // retail +464 (tag 44): SetScenarioPlayerID @0x2c5ba0
public:
	// retail table @0x2c6b10, member-by-member; tags 9/14/15 are retail's own permanent gaps.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nMoveInLastTurn); f.Add(3,&eLastAction); f.Add(4,&nLastActionTimes); f.Add(5,&criticals); f.Add(6,&_nShameOnEpik); f.Add(7,&bLogActive); f.Add(8,&nMotionless); f.Add(10,&savedSnipeAP); f.Add(11,&fLastToHit); f.Add(12,&lastCriticals); f.Add(13,&bUseTwoHanded); f.Add(16,&tohit); f.Add(17,&sAISoundConstants); f.Add(18,&SInterruptsConstants); f.Add(19,&ptLastCP); f.Add(20,&criticalsState); f.Add(21,&pkModifiers); f.Add(22,&pModel); f.Add(23,&pRPGUnit); f.Add(24,&sID); f.Add(25,&nBulletHitThisTurn); f.Add(26,&AckAttackers); f.Add(27,&AckIDs); f.Add(28,&diplomacy); f.Add(29,&bUnconscious); f.Add(30,&pPanzerklein); f.Add(31,&pPanzerkleinVP); f.Add(32,&bHiding); f.Add(33,&pMinesConstants); f.Add(34,&suspendedCriticals); f.Add(35,&nBleedingStopAmount); f.Add(36,&postponedModifiers); f.Add(37,&temporaryModifiers); f.Add(38,&vpBoostDurations); f.Add(39,&nTurnCounter); f.Add(40,&pAdaptatedWeapon); f.Add(41,&fAdaptationCounter); f.Add(42,&fCurrentAdaptation); f.Add(43,&fAuraPerkICModifier); f.Add(44,&nScenarioPlayerID); f.Add(45,&pGame); f.Add(46,&bIsAIUnit); f.Add(47,&pGlobalGame); return 0; }

	// retail +468 (tag 45): the per-mission RPG game, pushed in by the owning unit-server (retail
	// threads it through the ctor); the combat critical clamp honors CGame::nMaxCriticalSeverity.
	CPtr<IGame> pGame;
	virtual void SetGame( IGame *p ) { pGame = p; }
	// retail +472 (tag 46): SetIsAIPlayer @0x2c5b50; the ctor clears it.
	bool bIsAIUnit;
	// retail +476 pGlobalGame -- serialize tag 0x2f(47) @0x2c6b10, NOT 0x2c as the older comment
	// claimed (0x2c(44) is nScenarioPlayerID; the dev tag 34 it used to sit on is retail's
	// suspendedCriticals). Carries pDifficulty, read by CreateAttack's backstab-damage multipliers.
	// Retail threads it through the ctor (CreateUnit @0x2c4f50); this fork binds it in the
	// CUnitServer ctor (SetGlobalGame).
	CPtr<CGlobalGame> pGlobalGame;
	virtual void SetGlobalGame( CGlobalGame *p )
	{
		pGlobalGame = p;
		// This fork supplies the retail constructor's game/unit context when a
		// fresh server is created. Not serialized and not rebound during load.
		CUnitMissionForMedals::pGame = p;
		pMedalsGainer = pRPGUnit;
	}
	virtual void Segment() { CUnitMissionForMedals::Segment(); }
	virtual NDb::EDiplomacyState MedalDiplomacyState( int nPlayer )
	{
		return GetDiplomacy().GetDiplomacyState( nPlayer );
	}
	virtual CGlobalGame* GetGlobalGame() const { return pGlobalGame; }
	// retail @0x2c5b40/@0x2c5b50/@0x2c5b90/@0x2c5ba0/@0x2c5bb0 -- plain setters/getters.
	bool IsAIPlayer() const { return bIsAIUnit; }
	void SetIsAIPlayer( bool b ) { bIsAIUnit = b; }
	virtual void SetAuraPerkICModifier( float f ) { fAuraPerkICModifier = f; }
	void SetScenarioPlayerID( int n ) { nScenarioPlayerID = n; }
	int GetScenarioPlayerID() const { return nScenarioPlayerID; }
	// retail AddVPBoost @0x2c3400: install a temporary VP modifier (fAdd = 1% of the VP base per
	// unit of strength), record its lifetime and check overdose.
	virtual void AddVPBoost( float fStrength, int nDuration );   // overrides IUnitMission vtbl+0x184
	//
	CUnitMission();
	//
	virtual void GetInfo( NAI::EPose pose, SUnitInfo *pInfo ) const;
	virtual bool IsDead() const;
	virtual NDb::EWeaponType GetWeaponType() const;
	virtual NDb::CAnimWeaponType* GetDBAnimWeapon() const;
	virtual int  GetActionAP( NAI::EPose pose, EAction action ) const;
	virtual int GetAP() const;
	virtual bool CanSpendAP( int nAP ) const;
	virtual void SpendAP( int nAP );
	virtual void RegisterAction( EAction action );
	virtual const SSnipeAP& GetSavedAP() const { return savedSnipeAP; }
	virtual void SaveAP( const SSnipeAP &ap );
	virtual float GetSightDistance( NAI::EPose pose ) const;
	virtual void StartNewTurn( const CVec3 &ptCP );
	virtual bool IsFirstTurn() const { return nTurnCounter < 2; } // retail 0x6c5b80

	virtual bool CreateAttack( vector<CAttackPortion> *pRes, bool bSpendAmmo,
		bool bAnonymous, IUnitMissionInfo *pTarget, bool bBackStab, bool bAdaptWeapon );
	virtual CReceivedDmg ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
		NDb::CRPGArmor *pArmor );

	// GetToHit/GetTileToHit/GetObjectToHit/GetGrenadeToHit/GetRLauncherToHit moved to free fns
	// (NRPG::Get*ToHit, RPGUnitMission.h) -- the release migrated the to-hit dispatch out of the
	// interface. GetToHitWeaponType / GetSnipeAP / GetMeleeToHit stay as private helpers the free fns
	// reach via friendship.

	virtual int  GetBulletsQuantityInShot() const;
	virtual float HealVP( const SFirstAid &fa );
	virtual float HealCriticals( int nDC, bool bStopBleeding = true );
	virtual SHealCriticalInfo TryHealCritical( int nDC, bool bStopBleeding ) const;
	virtual float HealCritical( const SHealCriticalInfo &info );
	virtual void AddBleedingStopper( int nAmount ) { nBleedingStopAmount += nAmount; }
	virtual void AddPostponedModifier( CDynamicSkill *pSkill, const SSkillModifyInfo &info, int nTurns );
	virtual int  GetIC() const;
	int GetPerkModifiedIC( EAttackType attackType ) const;
	virtual bool CheckIC( EAttackType attackType );
	virtual float GetWeaponAdaptation() const;
	virtual int CheckInterrupt( const IUnitMission *pEnemy, bool bIsMutual, bool bWasShot );
	virtual int  GetInterrupt() const { return pRPGUnit->Skills(NDb::ST_INTERRUPT); }
	virtual NDb::CRPGArmor* GetRPGArmor() const;
	virtual NDb::CModel* GetModel() const { return pRPGUnit->pModel; }
	virtual const wstring& GetName() const;
	virtual void Kill();
	virtual IInventory* GetInventory() const { return pRPGUnit->GetInventory(); };
	virtual IInventoryInfo* GetInventoryInfo() const { return pRPGUnit->GetInventory(); };
	virtual int GetMaxExtraAP() const;
	virtual int GetSkillValue( NDb::ESkillType skill ) const;
	virtual int GetSkillMaxValue( NDb::ESkillType skill ) const { return pRPGUnit->Skills( skill ).GetMaxValue(); }
	virtual float GetSkillProgress( NDb::ESkillType skill ) const { return pRPGUnit->Skills( skill ).GetProgress(); }
	virtual void DumpStats() const;
	virtual void PrintLog( bool bPrint ) { bLogActive = bPrint; }
	virtual void ApplyMotionless() { ++nMotionless; }      // retail @0x2c5a70
	virtual void ReleaseMotionless() { --nMotionless; }    // retail @0x2c5a80
	virtual bool CanMove() const { return nMotionless == 0; }
	virtual void Reload();
	virtual bool LoadWeapon( IWeaponItemInfo *pWeapon, IClipItem *pClip );
	virtual bool UnloadWeapon( IWeaponItemInfo *pWeapon );
	virtual void AddLastCritical( NDb::ECritical eCA ) { lastCriticals.push_back( eCA ); }
	virtual void GetLastCriticals( vector<NDb::ECritical> *pResCritical ) { *pResCritical = lastCriticals; lastCriticals.clear(); }
	virtual void UseTwoHanded( bool bUse ) { bUseTwoHanded = bUse;}
	virtual bool CanUseTwoHanded() const { return bUseTwoHanded; }
	virtual int GetRPGPersID() const { return pRPGUnit->GetRPGPersID(); }
	virtual void SetCannonItem( IWeaponItem *pItem );
	virtual IWeaponItem* GetCannonItem() const { return pRPGUnit->GetCannonItem(); }
	virtual IWeaponItem* GetWeaponItem() const { return pRPGUnit->GetWeaponItem(); }
	virtual IWeaponItemInfo* GetCannonItemInfo() const { return pRPGUnit->GetCannonItem(); }
	virtual bool CanHearSound( const CVec3 &ptSoundPosition, const CVec3 &ptListenerPosition,
		const NDb::SAISound &sound, IUnitMission *pSource );
	virtual int GetHearingProbability( IUnitMission *pSource, float fDist, const NDb::SAISound &sound, bool *pAudible );
	virtual CVec3 GetTurnStartCP() const { return ptLastCP; }
	virtual NDb::CRPGPers* GetRPGPers() const;
	virtual NDb::CComplexHead* GetRPGPersHead() const;
	virtual CUnit *GetRPGUnit() const { return pRPGUnit; }
	virtual SRandomSeed GetBindPlacesSeed() const { return pRPGUnit->GetBindPlacesSeed(); }
	virtual NDb::SToHitConstants *GetToHitConstants() { return &tohit; }
	virtual NDb::SAISoundConstants *GetAISoundConstants() { return &sAISoundConstants; }
	virtual NDb::SInterruptsConstants *GetInterruptsConstants() { return &SInterruptsConstants; }
	virtual bool HasCritical( NDb::ECritical eCritical, CCritical** ppCritical = 0 ) const;
	virtual bool HasCurableCriticals() const;
	virtual void ApplyCritical( const SCritical &critical );
	virtual bool RemoveCritical( NDb::ECritical eCritical );
	virtual void SuspendCriticals( int nTurns );
	virtual void GetCriticalsList( list<CPtr<ICriticalInfo> > *pListCriticals ) const;
	virtual void MakeDirectDamage( int nDmg );
	virtual void EnableCriticals();
	virtual void DisableCriticals();
	virtual void DisableCritical( NDb::ECritical eC, ECriticalState eState  );
	virtual void BulletHit() { ++nBulletHitThisTurn; }
	virtual bool GetAck( int *pAckID, IUnitMissionInfo ** ppAttacker );
	virtual float GetXP( int nHowManyPerson ) const;
	virtual void StartRealTime();
	virtual const SDiplomacy& GetDiplomacy() const { return diplomacy; }
	virtual void SetDiplomacy( const SDiplomacy &dip ) { diplomacy = dip; }
	virtual bool IsUnconscious() { return bUnconscious; }
	virtual void InitAsCorpse( bool bDead );
	virtual int GetFallDamage( float fHDiff );
	virtual NDb::CPanzerklein *GetPanzerklein() { return pPanzerklein; }
	virtual void SetPanzerklein( NDb::CPanzerklein *pPK, CDynamicSkill *_pPanzerkleinVP, IInventory *_pPKInventory );
	void InitAsPanzerklein( NDb::CPanzerklein *pPK );   // retail @0x2c4f50 tail (CreateUnit PK-pers VP init)
	virtual void DoRegenerations( NWorld::IWorld *pWorld, int *pnBleed );
	virtual bool IsHero() const;
	virtual bool IsHiding() const { return bHiding; }
	virtual void SetHiding( bool _bHiding );
	virtual bool HasPerk( int nPerkID, 
		float *pParam1 = 0, float *pParam2 = 0, float *pParam3 = 0 ) const;
	virtual int GetGrenadeTrapDC( NDb::CRPGGrenade *pGrenade );
	virtual int GetMineDC( NDb::CRPGMine *pMine );
	virtual bool CanSeeMine( float fDistance, int nDC );
	virtual float GetMineSpotRange( int nDC );
	virtual bool CanClear( int nDC, int nSkillModif );
	virtual int GetUnhideProbability( IUnitMission *pTarget, float fDistance, bool bNight ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DBCriticalCmp( const SCriticalType &a, const SCriticalType &b )
{
	return a.nStartPos < b.nStartPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// place the criticals in the list in the order specified by the user
void RangeCriticals( vector<SCriticalType> *pCriticals )
{
	if ( pCriticals->empty() )
		return;
	sort( pCriticals->begin(), pCriticals->end(), DBCriticalCmp );
	//
	int pos = 0;
	for ( vector<SCriticalType>::iterator i = pCriticals->begin(); i != pCriticals->end(); ++i )
	{
		i->nStartPos = pos;
		pos += i->pCritical->nRange;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitializeCriticals()
{
	CDBTable<NDb::CRPGCritical> *pDTable = NDatabase::GetTable<NDb::CRPGCritical>();
	CDBIterator<NDb::CRPGCritical> it(*pDTable);
	while ( it.MoveNext() )
	{
		NDb::CRPGCritical *pC = it.Get();
		if ( !pC )
			continue;
		ASSERT( pC->hl < NDb::N_CL );
		if ( pC->hl >= NDb::N_CL )
			continue;
		criticalBar[pC->hl].push_back( SCriticalType( pC->nWeight, pC ) );
	}
	for ( int i = 0; i < NDb::N_CL; ++i )
		RangeCriticals( &criticalBar[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitMission::CUnitMission()
	: nMoveInLastTurn(0), nLastActionTimes(0), bLogActive(true),
		nMotionless(0), fLastToHit(0),
		bUseTwoHanded(true), nBulletHitThisTurn(0),
		bUnconscious( false ), bHiding( false ),
		nBleedingStopAmount( 0 ), nTurnCounter( 0 ), fAdaptationCounter( 0 ), fCurrentAdaptation( 0 ),
		fAuraPerkICModifier( 0 ), nScenarioPlayerID( 0 ), bIsAIUnit( false )   // retail ctor @0x2c57c0 zero-inits + clears the AI flag
{
	pMinesConstants = NDb::GetDBMinesConstants();
	ASSERT( IsValid( pMinesConstants ) );
	NDb::CRPGToHit *pToHit = NDb::GetToHitConstants( 1 );
	if ( pToHit )
		tohit = pToHit->constants;
	NDb::CRPGAISoundConstants *pAISoundConstants = NDb::GetAISoundConstants( 1 );
	if ( pAISoundConstants )
		sAISoundConstants = pAISoundConstants->constants;
	NDb::CRPGInterruptsConstants *pInterruptsConstants = NDb::GetInterruptsConstants( 1 );
	if ( pInterruptsConstants )
		SInterruptsConstants = pInterruptsConstants->constants;

	criticalsState.resize( NDb::N_CRIT_TYPES );
	EnableCriticals();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CUnitMission::GetName() const
{
	return pRPGUnit->GetName();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::InitAsCorpse( bool bDead )
{
	bUnconscious = !bDead;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::HasPerk( int nPerkID, 
		float *pParam1, float *pParam2, float *pParam3 ) const
{
	return pRPGUnit->HasPerk( nPerkID, pParam1, pParam2, pParam3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SaveAck( int nAckID, IUnitMissionInfo *pAttacker )
{
	AckAttackers.push_back( pAttacker );
	AckIDs.push_back( nAckID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::GetAck( int *pAckID, IUnitMissionInfo **ppAttacker )
{
	if ( AckAttackers.empty() )
		return false;
	//
	*pAckID = AckIDs.front();
	AckIDs.pop_front();
	*ppAttacker = AckAttackers.front().Extract();
	AckAttackers.pop_front();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetUnhideProbability( IUnitMission *pTarget, float fDistance, bool bNight ) const
{
	// Retail v1.2 0x6bf7a0: preserve x87 precision until each truncating skill conversion.
	int nSpot = GetRPGUnit()->Skills( NDb::ST_SPOT );
	float fParam;
	if ( HasPerk( N_PERK_BETTER_SPOTTING, &fParam ) )
		nSpot = int( ( 1.0 + double( fParam ) ) * nSpot );
	int nBase = int( 45.0 - double( 44.f / 30.f ) * fDistance );
	int nStealth = pTarget->GetRPGUnit()->Skills( NDb::ST_STEALTH );
	// ORIGINAL RETAIL BUG FIXED (perk 75): night camouflage belongs to the
	// hidden target and improves its stealth only at night, not the observer's
	// detection in both day and night.
	if ( bNight && pTarget->HasPerk( N_PERK_HIDE_IN_NIGHT, &fParam ) )
		nStealth = int( ( 1.0 + double( fParam ) ) * nStealth );
	return Clamp( nBase + nSpot - nStealth, 0, 100 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetSkillValue( NDb::ESkillType skill ) const
{
	int nSkill = pRPGUnit->Skills( skill );
	// ORIGINAL RETAIL BUG FIXED (perk 70): actual melee accuracy/defence,
	// damage and AP costs must use the advertised +10, not just the unused
	// derived GetWeaponSkill branch. Do not modify or serialize the base skill.
	float fBonus;
	if ( skill == NDb::ST_MELEE && HasPerk( 70, &fBonus ) )
		nSkill = int( nSkill + fBonus );
	return nSkill;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::UseSkill( NDb::ESkillType eSkill )
{
	if ( !pRPGUnit->UseSkill( eSkill ) )
		return;
	SaveAck( NWorld::N_ACK_SKILL, 0 );
	csRPG << "<font size=16pt>";
	csRPG << "<color=yellow>" << GetName() << " improve skill N" << eSkill << " to " << pRPGUnit->Skills(eSkill) << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::GetInfo( NAI::EPose pose, SUnitInfo *pInfo ) const
{
	pInfo->nAP = pRPGUnit->Skills(NDb::ST_AP);
	pInfo->nMaxAP = pRPGUnit->Skills(NDb::ST_AP).GetMaxValue();
	pInfo->nHP = pRPGUnit->Skills(NDb::ST_VP);
	pInfo->nMaxHP = pRPGUnit->Skills(NDb::ST_VP).GetMaxValue();
	pInfo->nSightDistance = int( GetSightDistance( pose ) );
	pInfo->nHealedHP = GetHealedVP();

	pInfo->bWearingPK = pPanzerklein && pPanzerkleinVP;
	pInfo->nPKLife = pInfo->bWearingPK ? int( *pPanzerkleinVP ) : 0;
	pInfo->nMaxPKLife = pInfo->bWearingPK ? pPanzerkleinVP->GetMaxValue() : 0;
	pInfo->bPKInfo = pInfo->bWearingPK;
	pInfo->nPKHP = pInfo->nPKLife;
	pInfo->nMaxPKHP = pInfo->nMaxPKLife;
	pInfo->bUnitInfo = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetSnipeAP( IUnitMissionInfo *pTarget ) const
{
	if ( savedSnipeAP.pTarget == pTarget )
		return savedSnipeAP.nAP;
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::IsDead() const
{
	return pRPGUnit->IsDead();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::MakeDirectDamage( int nDmg )
{
	pRPGUnit->Skills(NDb::ST_VP) -= nDmg;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::RegisterAction( EAction action )
{
	if ( action == AC_PREPARE_AND_SHOOT )
	{
		action = AC_SHOOT;
		nLastActionTimes = 0;
	}
    eLastAction = action;

	if ( action != AC_END_SHOOT && action != AC_SHOOT && action != AC_BURST )
		nLastActionTimes = 0;

	switch( action )
	{
		case AC_END_SHOOT:
			nLastActionTimes++;
			break;
		case AC_MELEE:
			UseSkill( NDb::ST_MELEE );
			break;
		case AC_THROW_GRENADE:
		case AC_THROW_KNIFE:
			UseSkill( NDb::ST_THROWING );
			break;
		case AC_SHOOT:
		case AC_PREPARE_AND_SHOOT:
			UseSkill( NDb::ST_SHOOTING );
			break;
		case AC_BURST:
			UseSkill( NDb::ST_BURST );
			break;
		case AC_HIDE:
			// Retail v1.2 0x6c218f: entering Hide practices Stealth, with upgrade ack/log.
			UseSkill( NDb::ST_STEALTH );
			break;
		case AC_FIRSTAID:
			// Retail v1.2 0x6c217d: practice when first aid is initiated.
			UseSkill( NDb::ST_MEDICINE );
			break;
		case AC_TRAP_OBJECT:
		case AC_DISARM_TRAP:
		case AC_SET_MINE:
		case AC_DISARM_MINE:
		case AC_PICK_LOCK:
		case AC_REPAIR_PK:
			// Retail v1.2 0x6c21a1: all six engineering actions share this handler.
			UseSkill( NDb::ST_ENGINEERING );
			break;
		case AC_MOVE_DIAGONAL:
		case AC_MOVE_SIDE:
			// retail @0x34edb0: the nMoveInLastTurn accounting moved to DoAction (gated on RUN pose); here we
			// only spend the AP-skill use. (Old code added it unconditionally here, with a diagonal fall-through
			// bug that summed +3 -- both are corrected by the relocation.)
			UseSkill( NDb::ST_AP );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetFallDamage( float fHDiff ) 
{
	// crap
	int nHit = ( fHDiff - 2.8f ) * 50;
	int nRet = 0;
	for ( int i = 0; i < 3; ++i )
		nRet += random.Get( 0, nHit / 4 ); 
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SpendAP( int nAP )
{
	if ( GetRPGUnit()->IsCheatEnabled( CHEAT_AP ) ||
		GetRPGUnit()->IsCheatEnabled( CHEAT_SCRIPTSEQUENCE ) )
			return;
	//
	// ORIGINAL RETAIL BUG FIXED (v1.1 0x6bf810 / v1.2 0x6bf9c0):
	// subtracting a negative action cost awards AP. Preserve explicit zero-cost
	// calls (free perks, no extra aiming AP), but never treat spending as a refund.
	if ( nAP < 0 )
		nAP = 1;
	ASSERT( pRPGUnit->Skills(NDb::ST_AP) >= nAP );
	pRPGUnit->Skills(NDb::ST_AP) -= nAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::CanSpendAP( int nAP ) const
{
	if ( GetRPGUnit()->IsCheatEnabled( CHEAT_AP ) ||
		GetRPGUnit()->IsCheatEnabled( CHEAT_SCRIPTSEQUENCE ) )
			return true;
	//
	if ( nAP < 0 )
		nAP = 1; // Keep affordability consistent with SpendAP's negative-cost guard.
	return pRPGUnit->Skills(NDb::ST_AP) >= nAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetAP() const
{
	return pRPGUnit->Skills(NDb::ST_AP);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::GetSightDistance( NAI::EPose pose ) const
{
	float fRetDist = 0;
	//NAI::EPose pose = pUnit->GetPosition().GetPose();
	switch ( pose )
	{
		case NAI::CRAWL: fRetDist = 25; break;
		case NAI::CROUCH: fRetDist = 27; break;
		case NAI::WALK: fRetDist = 30; break;
		case NAI::RUN: fRetDist = 30; break;
	}
	return fRetDist * FP_GRID_STEP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::ResetUnitParameters()
{
	nBulletHitThisTurn = 0;
	pRPGUnit->Skills(NDb::ST_AP).Reset();
	nMoveInLastTurn = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::StartNewTurn( const CVec3 &ptCP )
{
	++nTurnCounter;               // retail @0x2bf8c0 head
	ResetUnitParameters();
	ptLastCP = ptCP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::StartRealTime()
{
	ResetUnitParameters();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetActionAP( NAI::EPose curPose, EAction action ) const
{
	// ORIGINAL RETAIL BUG FIXED: paid costs have a 1-AP floor AFTER skill and
	// perk reductions, so UI/AI planning and execution agree. Explicitly free
	// preparation/turning, no-op actions and missing-item sentinels stay zero.
	// actions whose cost does not depend on the pose
	switch ( action )
	{
		case AC_NONE: return 0;
		case AC_HIDE: return 14;
		case AC_PREPARE: 
		{
			// Retail v1.2 0x6c0f81..0x6c1010: base preparation depends on the
			// weapon type. Its original zero-parameter perk bug is corrected below.
			int nAP = 0;
			CWeaponItem *pWeapon = pRPGUnit->GetWeaponItem();
			if ( IsValid( pWeapon ) )
				nAP = pWeapon->GetDBWeapon()->pWeaponType->nPrepareCost;
			if ( nAP <= 0 )
				return 0; // No weapon, or a weapon type that needs no preparation.
			float fParam;
			if ( HasPerk( N_PERK_CHEAP_SHOOT_PREPARE, &fParam ) )
			{
				// ORIGINAL RETAIL BUG FIXED (perk 22): Param1=0 describes free
				// preparation; subtracting zero left the entire AP cost in place.
				if ( fParam == 0 )
					return 0;
				nAP = int( nAP * fParam );
			}
			return Min( 8, Max( 1, nAP ) );
		}
		case AC_SHOOT:   return pRPGUnit->GetWeaponAP();
		case AC_PREPARE_AND_SHOOT:	return GetActionAP( curPose, AC_PREPARE ) + GetActionAP( curPose, AC_SHOOT );
		case AC_EXPLODE: return 2;
		case AC_THROW_GRENADE:
		{
			// Retail v1.2 0x6c10f8: RPGAP record 5, reduced by perk 0x2b.
			// Keep x87 intermediate precision until the final truncating conversion.
			NDb::CRPGAP *pAP = NDb::GetRPGAP( 5 );
			int nAP = pAP ? pAP->nAP : 0;
			if ( nAP == 0 )
				return 0; // Preserve a missing record or an explicitly free DB cost.
			float fParam;
			if ( HasPerk( N_PERK_CHEAP_THROW, &fParam ) )
				nAP = int( ( 1.0 - double( fParam ) ) * nAP );
			return Max( 1, nAP );
		}
		case AC_CLIMB_1: return 10;
		case AC_CLIMB_2: return 12;
		case AC_CLIMB_3: return 14;
		case AC_CLIMB_4: return 15;
		case AC_JUMP: return 10;
		case AC_TAKE_CORPSE: return 12;
		case AC_TRAP_OBJECT: return 30;
		case AC_DISARM_TRAP: return 30;
		case AC_SET_MINE: 
			{
				NRPG::IInventoryItem *pItem = pRPGUnit->GetInventory()->GetActive();
				CDynamicCast<NRPG::IMineItem> pMine(pItem);
				if (pMine)
					return pMine->GetDBItemInfo()->nAPToSet;
			}
			return 0;
		case AC_DISARM_MINE:
			return 30;

		case AC_FIRSTAID: 
			{
				NRPG::IFirstAidItem *pItem = pRPGUnit->GetFirstAidItem();
				if ( pItem )
					return pItem->GetDBFirstAid()->nAPToUse;
				ASSERT( 0 && "manual therapist?" );
				return 15;
			}
		case AC_THROW_KNIFE:
		case AC_MELEE:
			{
				CMeleeWeaponItem *pMelee = pRPGUnit->GetMeleeWeaponItem();
				if ( !pMelee )
					return 0;
				NDb::CRPGMeleeWeapon *pW = pMelee->GetDBMeleeWeapon();
				NDb::ESkillType skill = action == AC_THROW_KNIFE ? NDb::ST_THROWING : NDb::ST_MELEE;
				int nAP = pW->nMaxAP - (pW->nMaxAP - pW->nMinAP) * GetSkillValue( skill ) / N_MAX_SKILL;
				float fParam;
				if ( HasPerk( action == AC_THROW_KNIFE ? N_PERK_CHEAP_THROW : N_PERK_CHEAP_MELEE, &fParam ) )
					nAP = int( ( 1.0 - double( fParam ) ) * nAP );
				// Retail v1.1 0x6c11b1 / v1.2 0x6c1347 extrapolate below zero
				// at high skills. Floor the final cost, not the skill itself.
				return Max( 1, nAP );
			}
		case AC_BURST:
			return pRPGUnit->GetWeaponBurstAP();
		case AC_RELOAD:
		{
			int nAP = pRPGUnit->GetWeaponReloadAP();
			if ( nAP == 0 )
				return 0; // No firearm to reload.
			float fParam;
			// ORIGINAL RETAIL BUG FIXED (perk 33): the stated 30% reduction
			// means paying 70% of base AP, not 30% as in retail 0x6c1443.
			if ( HasPerk( N_PERK_CHEAP_RELOAD, &fParam ) )
				nAP = int( nAP * ( 1.0f - fParam ) );
			return Max( 1, nAP );
		}
		case AC_OPEN_CLOSE:
			return 4;
		case AC_APPROACH_CANNON:
			return 4;

		case AC_POSE_CRAWL:
		case AC_POSE_CROUCH:
		case AC_POSE_WALK:
		case AC_POSE_RUN:
		{
			if ( action == AC_POSE_RUN )
				action = AC_POSE_WALK;
			if ( curPose == NAI::RUN )
				curPose = NAI::WALK;
			int nCoeff = 4;
			//
			float fParam;
			if ( HasPerk( N_PERK_CHEAP_CHANGE_POSE, &fParam ) )
				nCoeff -= fParam;
			//
			int nChanges = abs( curPose - ( action - AC_POSE_CRAWL ) );
			return nChanges == 0 ? 0 : Max( 1, nChanges * nCoeff );
		}
		case AC_LADDER:
			return 2;
		case AC_LADDER_MOVE:
			return 2;
		case AC_END_SHOOT:
			return 0;
		case AC_ENTER_PK:
			return 10;
		case AC_LEAVE_PK:
			return 6;
		case AC_ROTATE:
			{
				float fParam;
				if ( HasPerk( N_PERK_CHEAP_ROTATE, &fParam ) )
					return fParam == 0 ? 0 : Max( 1, int( fParam ) );
				else
					return curPose == NAI::CRAWL? 4 : 2;
			}
		// @0x2c0bd0 -- retail sources these from the RPGAP DB table (NDb::GetRPGAP, ids 11/12/13)
		case AC_ITEM_TAKE:
		case AC_SWAP:
		{
			NDb::CRPGAP *pAP = NDb::GetRPGAP( action == AC_SWAP ? 10 : 11 );
			return pAP ? pAP->nAP : 0;
		}
		case AC_ITEM_SLOT:
		{
			NDb::CRPGAP *pAP = NDb::GetRPGAP( 12 );
			return pAP ? pAP->nAP : 0;
		}
		case AC_ITEM_TRANSFER:
		{
			NDb::CRPGAP *pAP = NDb::GetRPGAP( 13 );
			return pAP ? pAP->nAP : 0;
		}
		// @0x2c0bd0 -- the locked-door actions (CExecOpenClose::GetStartAP @0x3bd340)
		case AC_USE_KEY:
		{
			NDb::CRPGAP *pAP = NDb::GetRPGAP( 9 );        // RPGAP record 9 = USE_KEY
			return pAP ? pAP->nAP : 0;
		}
		case AC_PICK_LOCK:
		{
			// retail: the cost comes off the ACTIVE picklock's own record (nAPToUse), 0 when none held
			CDynamicCast<IPicklockItem> pPick( GetInventory()->GetActive() );
			if ( IsValid( pPick ) )
				return pPick->GetDBPicklock()->nAPToUse;
			return 0;
		}
	}

	// actions whose cost depends on the pose
	// Retail 0x6c1680/0x6c1732: substitutions recurse (34 and 35 can combine).
	if ( curPose == NAI::CRAWL && HasPerk( N_PERK_PRONE_COSTS_CROUCH ) )
		return GetActionAP( NAI::CROUCH, action );
	if ( curPose == NAI::CROUCH && HasPerk( N_PERK_CROUCH_COSTS_WALK ) )
		return GetActionAP( NAI::WALK, action );
	int nAddedAP = 0;
	if ( pPanzerklein )
	{
		// Retail 0x6c1746..0x6c1830: after pose-perk substitutions, unsupported
		// prone/crouched PK movement returns 1 before any corpse-cost perk is queried.
		if ( curPose == NAI::CRAWL || curPose == NAI::CROUCH )
			return 1;
		nAddedAP = pPanzerklein->nAddMoveAP;
	}
	switch ( curPose )
	{
		case NAI::CRAWL:
		case NAI::CROUCH:
		case NAI::WALK:
		case NAI::RUN:
		{
			int nPose = ( NAI::RUN - curPose + 1 ) * 2;
			float fCorpseAP;
			if ( ( action == AC_MOVE_CORPSE_SIDE || action == AC_MOVE_CORPSE_DIAGONAL ) &&
				HasPerk( N_PERK_CORPSE_TRACKER, &fCorpseAP ) )
				return Max( 1, int( action == AC_MOVE_CORPSE_SIDE ? double( fCorpseAP ) : double( fCorpseAP ) * 1.5 ) );
			switch ( action )
			{
				case AC_MOVE_SIDE:		return Max( 1, nPose + nAddedAP );
				case AC_MOVE_DIAGONAL:	return Max( 1, ( ( nPose + nAddedAP ) * 15 ) / 10 );
				case AC_MOVE_CORPSE_SIDE:		return Max( 1, ( ( nPose + nAddedAP ) * 15 ) / 10 );
				case AC_MOVE_CORPSE_DIAGONAL:	return Max( 1, ( ( nPose + nAddedAP ) * 225 ) / 100 );
			}
		}
		break;
	}
	ASSERT(0); // unknown action
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CAnimWeaponType* CUnitMission::GetDBAnimWeapon() const
{
	return pRPGUnit->GetDBAnimWeapon();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CUnitMission::GetWeaponType() const 
{
	return pRPGUnit->GetWeaponType();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EToHitType CUnitMission::GetToHitWeaponType() const
{
	IWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
	{
		NDb::CRPGWeapon *pDBWeapon = pWeapon->GetDBWeapon();
		if ( pDBWeapon->bBazookaLogic )
			return TH_RLAUNCHER;
		return TH_SHOOT;
	}
	IMeleeWeaponItem *pMeleeWeapon = pRPGUnit->GetMeleeWeaponItem();
	if ( pMeleeWeapon )
	{
		NDb::CRPGMeleeWeapon *pDBMWeapon = pMeleeWeapon->GetDBMeleeWeapon();
		if ( pDBMWeapon->bThrowing )
			return TH_THROWING;
		return TH_MELEE;
	}
	if ( CDynamicCast<IGrenadeItem>( pRPGUnit->GetInventory()->GetActive() ) )
		return TH_GRENADE;
	return TH_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DumpAttackPortion( const CAttackPortion &ap )
{
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << " \tK="  << CC_GREY << ap.nK;
	csRPG << CC_ORANGE << " \tType="    << CC_GREY << ap.nDmgType;
	csRPG << CC_ORANGE << " \tDmgMin=" << CC_GREY << ap.nDmgMin;
	csRPG << CC_ORANGE << " \tDmgMax=" << CC_GREY << ap.nDmgMax;
	csRPG << CC_ORANGE << " \tCrit="  << CC_GREY << ap.nCrtical;
	csRPG << CC_ORANGE << " \tCritSev=" << CC_GREY << ap.nCrticalDifficulty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetUnconsciousProbability( IUnitMissionInfo *pAttacker, 
	IUnitMissionInfo *pTarget, int nBaseProbability ) const
{
	if ( !IsValid( pTarget ) )
		return nBaseProbability;
	//
	int nHits = pTarget->GetSkillValue( NDb::ST_VP );
	int nMaxHits = pTarget->GetSkillMaxValue( NDb::ST_VP );
	return ( - 1.f / ( 2.f * nMaxHits ) * nHits + 1 ) * nBaseProbability;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2c2100: returns whether an attack was created (melee branch: always true). The Jan03
// "burst not finished -> false" ShootMode switch is GONE in retail -- burst pacing lives entirely
// in the shoot exec's timed-bullet schedule (nBulletGone / tNextBullet*), and no live caller reads
// the return on the shoot path.
bool CUnitMission::CreateAttack( vector<CAttackPortion> *pRes, bool bSpendAmmo,
	bool bAnonymous, IUnitMissionInfo *pTarget, bool bBackStab, bool bAdaptWeapon )
{
	IUnitMissionInfo *pAttacker = bAnonymous ? 0 : this;
	bool bRet = true;
	CWeaponItem *pWeapon  = pRPGUnit->GetWeaponItem();
	CMeleeWeaponItem *pMW = pRPGUnit->GetMeleeWeaponItem();
	// retail @0x6c216a (function head): the "Better critical difficulty" perk (id 44) scales EVERY
	// portion's nCrticalDifficulty (both ranged and melee) by its Param1 (=1.25). Fetched once here.
	float fCritDiffCoeff = 1.f;
	HasPerk( N_PERK_BETTER_CRIT_DIFFICULTY, &fCritDiffCoeff );
	// the item whose familiarity the adaptation tail updates (retail pIVar17: the branch's weapon)
	IInventoryItem *pUsedItem = 0;
	//
	if ( IsValid( pWeapon ) )
	{
		if ( pWeapon->GetDBWeapon()->pWeaponType->bTwoHanded && !CanUseTwoHanded() )
		{
			// we currently cannot use a two-handed weapon
			return true;
		}
		// retail @0x6c21cc/@0x6c221b (weapon-branch head): "+ N Ranged Dmg" (perk 26, flat add applied
		// below when damage lands) and "Better critical chance" (perk 36, nCrtical x(Param1+1)).
		int nRangedDmgAdd = 0;
		float fPerkDmg = 0;
		if ( HasPerk( N_PERK_RANGED_DMG_ADD, &fPerkDmg ) )
			nRangedDmgAdd = (int)fPerkDmg;
		float fCritChanceMult = 1.f;
		float fPerkCrit = 0;
		if ( HasPerk( N_PERK_BETTER_CRIT_CHANCE, &fPerkCrit ) )
			fCritChanceMult = fPerkCrit + 1.f;
		//
		pWeapon->CreateNewAttackPortion( pRes, bSpendAmmo );
		for ( int i = 0; i < pRes->size(); ++i )
		{
			int nSnipeSkill = pRPGUnit->Skills( NDb::ST_SNIPE );
			CAttackPortion &a = (*pRes)[i];
			a.pAttacker = pAttacker;
			a.pTarget = pTarget;
			// retail @0x6c22d8..0x6c2354: the crit chance is keyed on the TARGET's bullet-hits-this-turn
			// counter (repeat hits open him up), falling back to a flat 10 for a non-mission target; the
			// crit difficulty base is 0.25 * snipe skill (dev had the attacker's counter and 0.1 * snipe).
			CUnitMission *pTargetMission = CDynamicCast<CUnitMission>( pTarget );
			if ( IsValid( pTargetMission ) )
				a.nCrtical = (int)( ( pTargetMission->nBulletHitThisTurn + 1 ) * 10.f );
			else
				a.nCrtical = 10;
			a.nCrticalDifficulty = (int)( nSnipeSkill * 0.25f );
			a.nUnconsciousProbability = GetUnconsciousProbability( pAttacker,
				pTarget, pWeapon->GetInnerClip()->GetDBAmmo()->nUnconsciousProbability );
			a.bBackStab = bBackStab;
			//
			if ( IsValid(savedSnipeAP.pTarget) && pTarget == savedSnipeAP.pTarget )
			{
				// sniper shot: retail @0x6c2405..0x6c24ef -- crit += snipe*AP*0.02, crit difficulty += AP
				// (dev had AP*0.6 on both), then the "Master Sniper" perk (54) boosts both.
				a.nCrticalDifficulty += savedSnipeAP.nAP;
				a.nCrtical = (int)( nSnipeSkill * savedSnipeAP.nAP * 0.02f + a.nCrtical );
				if ( bSpendAmmo )
					UseSkill( NDb::ST_SNIPE );
				float fSniperCrit = 0, fSniperCritDiff = 0;
				if ( HasPerk( N_PERK_MASTER_SNIPER, &fSniperCrit, &fSniperCritDiff ) )
				{
					a.nCrtical = (int)( ( fSniperCrit + 1.f ) * a.nCrtical );
					a.nCrticalDifficulty = (int)( a.nCrticalDifficulty * fSniperCritDiff );
				}
			}
			else
			{
				// ordinary shot: retail @0x6c24f1 -- 0.4 * snipe skill (dev had /10)
				a.nCrtical = (int)( nSnipeSkill * 0.4f + a.nCrtical );
			}
			// retail @0x6c252b/@0x6c2566: the perk-36 crit-chance factor, then the perk-44 factor.
			a.nCrtical = (int)( a.nCrtical * fCritChanceMult );
			a.nCrticalDifficulty = (int)( a.nCrticalDifficulty * fCritDiffCoeff );
			// retail @0x6c2583..0x6c25c3: only when the portion actually deals damage -- the flat
			// perk-26 add on both bounds and "Always critical" (perk 38).
			if ( a.nDmgMin > 0 && a.nDmgMax > 0 )
			{
				a.nDmgMin += nRangedDmgAdd;
				a.nDmgMax += nRangedDmgAdd;
				if ( HasPerk( N_PERK_ALWAYS_CRITICAL ) )
					a.nCrtical = 100;
			}
			ModifyUnawareCritical( a, bBackStab );
		}
		pUsedItem = pWeapon;
	}
	else if ( pMW )
	{
		NDb::CRPGMeleeWeapon *pW = pMW->GetDBMeleeWeapon();
		// retail @0x6c25f3..0x6c2616: all-zero ctor (the Jan03 "Epik 110" is gone; nK and the
		// damage bounds are rebuilt below); fPushCoeff=0 -- melee kills never push the corpse.
		pRes->push_back( CAttackPortion( 0, 2, 0.0f, 0, 0, 0, 0 ) );
		CAttackPortion &a = pRes->front();
		// retail @0x6c264c: melee gib enable -- ONLY a Panzerklein pilot's strike may gib
		// (every other attack source keeps the ctor's bNoBlowUp=true).
		a.bNoBlowUp = pPanzerklein == 0;
		const int nStr = pRPGUnit->Skills( NDb::ST_STR );
		int nMelee = GetSkillValue( NDb::ST_MELEE ); // perk 70, including actual damage
		// retail @0x6c26c2: backstab multiplies the melee skill by the DIFFICULTY record's
		// fBackstabMeleeMultiplier (pGlobalGame->pDifficulty), not the old F_BACKSTAB_MELEE_COEFF
		// constant -- so backstab strength is difficulty-tunable. Truncating (int) matches the fistp.
		if ( bBackStab && IsValid( pGlobalGame ) && IsValid( pGlobalGame->pDifficulty ) )
			nMelee = (int)( nMelee * pGlobalGame->pDifficulty->fBackstabMeleeMultiplier );
		//
		a.nDmgMin = pW->nDmgMin + nStr + nMelee * (pW->nDmgMax - pW->nDmgMin) / (N_MAX_SKILL * 2);
		a.nDmgMax = pW->nDmgMax + nStr;
		// retail @0x6c2750: the backstab DAMAGE boost -- the (multiplied) melee skill scaled by the
		// difficulty record, halved for bare fists (melee record id 1), times 5/7, added to BOTH bounds.
		// ORIGINAL BUG (confirmed @0x6c2795/@0x6c27cd): retail reads fBackstabMinDamageMult for BOTH
		// bounds -- fBackstabMaxDamageMult is never read.
		if ( bBackStab && IsValid( pGlobalGame ) && IsValid( pGlobalGame->pDifficulty ) )
		{
			const float fFists = ( pW->GetRecordID() == 1 ) ? 0.5f : 1.0f;
			const float fBoost = nMelee * pGlobalGame->pDifficulty->fBackstabMinDamageMult * fFists * ( 5.f / 7.f );
			a.nDmgMin = (int)( fBoost + a.nDmgMin );
			a.nDmgMax = (int)( fBoost + a.nDmgMax );
		}
		// retail nK ladder @0x6c2802..0x6c2858: bare fists (melee record id 1) hit at
		// 160 + 10*STR; a real melee weapon doubles that; a Panzerklein pilot's strike is a
		// flat 1600 (0x640); a throwing knife swung in melee is a flat 40 (0x28).
		a.nK = 160 + 10 * nStr;
		if ( pW->GetRecordID() != 1 )
			a.nK *= 2;
		if ( pPanzerklein )
			a.nK = 1600;
		if ( IsValid( pW ) && pW->bThrowing )
			a.nK = 40;
		a.nCrtical = Max( 0.f, 10 + 0.4f * (nMelee - 25) + pMW->GetDBMeleeWeapon()->nCriticalBonus );
		// retail @0x6c28bf: nCrticalDifficulty = nCrtical * fCritDiffCoeff * 0.5 (dev dropped the perk factor)
		a.nCrticalDifficulty = a.nCrtical * fCritDiffCoeff * 0.5f;
		a.pAttacker = pAttacker;
		a.pTarget = pTarget;
		a.nUnconsciousProbability = GetUnconsciousProbability( pAttacker,
			pTarget, pMW->GetDBMeleeWeapon()->nUnconsciousProbability );
		a.bBackStab = bBackStab;
		// retail @0x6c2971: "Always melee critical" (perk 37).
		if ( HasPerk( N_PERK_ALWAYS_MELEE_CRITICAL ) )
			a.nCrtical = 100;
		// retail @0x6c2988..0x6c2a58: "Rage" (perk 45) -- when the unit's total VP (current + bandaged)
		// ratio drops below Param1 (0.5), both damage bounds scale by Param2 (1.5).
		float fRageThreshold = 0, fRageMult = 0;
		if ( HasPerk( N_PERK_RAGE, &fRageThreshold, &fRageMult ) )
		{
			const int nMaxVP = pRPGUnit->Skills( NDb::ST_VP ).GetMaxValue();
			if ( nMaxVP > 0 && GetTotalVP() / (float)nMaxVP < fRageThreshold )
			{
				a.nDmgMin = (int)( a.nDmgMin * fRageMult );
				a.nDmgMax = (int)( a.nDmgMax * fRageMult );
			}
		}
		// retail @0x6c2a58: "+ N Melee Dmg" (perk 55) -- flat Param1 add on both bounds.
		float fMeleeDmgAdd = 0;
		if ( HasPerk( N_PERK_MELEE_DMG_ADD, &fMeleeDmgAdd ) )
		{
			a.nDmgMin = (int)( a.nDmgMin + fMeleeDmgAdd );
			a.nDmgMax = (int)( a.nDmgMax + fMeleeDmgAdd );
		}
		// retail @0x6c2ad0
		ModifyUnawareCritical( a, bBackStab );
		pUsedItem = pMW;
	}
	// retail @0x6c2ae8 (adaptation tail): on a real, ammo-spending attack (once per shot -- the shoot
	// execs pass bAdaptWeapon = nBulletGone==0, melee always true) update the weapon familiarity.
	// Retail v1.1 0x6c2b0c..0x6c2c3c / v1.2 0x6c2c1c..0x6c2d4c:
	// perk 6 changes growth/decay rate; perk 71 divides rate and scales the
	// counter cap; perk 87 also scales that cap. UseWeapon converts the counter
	// to an accuracy bonus with the DB multiplier (normally 0.1), not the rate.
	if ( bSpendAmmo && bAdaptWeapon && pUsedItem )
	{
		float fRate = 1.0f;
		float fFastRate = 0;
		if ( HasPerk( N_PERK_FAST_WEAPON_ADAPTATION, &fFastRate ) )
			fRate = fFastRate;
		float fSlowCapMult = 0, fSlowRateDiv = 0;
		const bool bSlowAdaptation = HasPerk( N_PERK_SLOW_ADAPTATION_BONUS, &fSlowCapMult, &fSlowRateDiv );
		if ( bSlowAdaptation )
			fRate /= fSlowRateDiv;
		int nCap = tohit.nMaxWeaponAdaptation;
		float fCapMult = 0;
		if ( HasPerk( N_PERK_ADAPTATION_BONUS, &fCapMult ) )
			nCap = (int)( nCap * fCapMult );
		if ( bSlowAdaptation )
			nCap = (int)( nCap * fSlowCapMult );
		pRPGUnit->UseWeapon( pUsedItem, fRate, nCap, tohit.fWeaponAdaptationMult );
	}
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x6bec20: Param1 scales chance, Param2 scales difficulty.
// Both float-to-int conversions truncate; the capped chance is not a multiplier.
void CUnitMission::ModifyUnawareCritical( CAttackPortion &a, bool bApply ) const
{
	if ( !bApply )
		return;
	float fBonus = 0, fDifficulty = 0;
	if ( !HasPerk( N_PERK_UNAWARE_CRITICAL, &fBonus, &fDifficulty ) )
		return;
	const float fCapped = Min( 100.0f, ( fBonus + 1.0f ) * a.nCrtical );
	a.nCrtical = int( fCapped );
	a.nCrticalDifficulty = int( a.nCrticalDifficulty * fDifficulty );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGArmor* CUnitMission::GetRPGArmor() const 
{
	if ( pPanzerklein )
		return pPanzerklein->pArmor;
	return NDb::GetArmor( NDb::N_HUMAN_BODY_ARMOR );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::GetXP( int nHowManyPerson ) const
{
	nHowManyPerson = Clamp( nHowManyPerson, 1, 6 );
	int nLvl = pRPGUnit->Skills(NDb::ST_LEVEL);
	float fXPDiff = pRPGUnit->GetXPForSkill( NDb::ST_LEVEL, nLvl+1 ) - pRPGUnit->GetXPForSkill( NDb::ST_LEVEL, nLvl );
	return ( fXPDiff / float(nHowManyPerson) ) / 8.1f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CReceivedDmg CUnitMission::ProcessAttackForPK( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
	NDb::CRPGArmor *pArmor, bool bApplyToVP )
{
	// Retail v1.2 0x6bfb70: a rejected hit, a pilot hit and a suit hit are
	// different outcomes, including when the suit takes zero damage.
	if ( pRPGUnit->IsCheatEnabled( CHEAT_GODMODE ) )
		return CReceivedDmg();
	if ( pAttack->bBypassPK )
		return CReceivedDmg( -1, RD_HUMAN );
	if ( pAttack->CanRicochet() && random.Check( pPanzerklein->nRicochetProb ) )
	{
		pAttack->nK = 0;
		return CReceivedDmg();
	}
	
	CDynamicSkill *pVP = 0;
	if ( bApplyToVP )
		pVP = &pRPGUnit->Skills(NDb::ST_VP);
	else 
		pVP = pPanzerkleinVP;
	CDynamicSkill &panzerkleinVP = *pVP;
	bool bWasAlive = panzerkleinVP > 0;

	const float fPilotFraction = 1.0f - pPanzerklein->fCriticalResist;
	const int nPilotCritical = int( fPilotFraction * pAttack->nCrtical );
	if ( random.Check( nPilotCritical ) )
	{
		// we damage only the pilot
		csRPG << CC_RED << " \tCritical, PK Ignored!" << endl;
		pAttack->nCrtical = nPilotCritical;
		pAttack->nDmgMax *= fPilotFraction;
		pAttack->nDmgMin *= fPilotFraction;
		return CReceivedDmg( -1, RD_HUMAN );
	}
	if ( !pAttack->CanDealDmg(pArmor) )
		return CReceivedDmg();
	int nDmg = pAttack->CalcStructDmg( pWorld, pArmor, 0 );
	if ( pAttack->atkType == AT_CLICK_OF_DEATH )
		nDmg = 100000;

	panzerkleinVP -= nDmg;
	if ( panzerkleinVP < 0 )
		panzerkleinVP.SetValue( 0 );
	csRPG << " \t" << CC_YELLOW << GetName() << CC_WHITE << " PK damaged on " << nDmg << "VP, remain " << panzerkleinVP << " PK VP" << endl;
	bool bAlive = panzerkleinVP > 0;
	if ( bWasAlive && !bAlive )
		WearBrokenPK();
	pAttack->nK = Max( pAttack->nK - 1590, 0 );
	return CReceivedDmg( nDmg, pAttack->nK > 0 ? RD_HUMAN : RD_PK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::WearBrokenPK()
{
	SCritical cr( NDb::CL_ANY, NDb::C_PANZERKLEIN_BROKEN );
	ApplyCritical( cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x6c3fd1: only a pilot outcome continues into human damage.
CReceivedDmg CUnitMission::ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
	NDb::CRPGArmor *pRealArmor )
{
	if ( GetRPGPers()->pPanzerklein ) // this is a PK on the zone and not on the pers, i.e. this pers is itself a PK
	{
		if ( !pPanzerklein )
			pPanzerklein =  GetRPGPers()->pPanzerklein;
		return ProcessAttackForPK( pWorld, nUserID, pAttack, pRealArmor, true );
	}
	NDb::CRPGArmor *pArmor = NDb::GetArmor( NDb::N_HUMAN_BODY_ARMOR );

	if ( GetPanzerklein() && *pPanzerkleinVP > 0 )
	{
		CReceivedDmg damage = ProcessAttackForPK( pWorld, nUserID, pAttack, pRealArmor, false );
		if ( damage.type != RD_HUMAN )
			return damage;
	}

	int nTotalDmg = 0;
	bool bAlive = !IsDead();
	if ( pAttack->CanDealDmg(pArmor) && pAttack->nK > 0 )
	{
		csRPG << "<font size=16pt>";
		// Retail v1.2 0x6c4067: contact accuracy was resolved by the ray path.
		// There is no early accuracy reroll; incidental-hit evasion reduces damage below.
		int nDmg = pAttack->CalcStructDmg( pWorld, pArmor, 0 );
		//	Get
		float fDmgModifier = 0;
		switch ( nUserID )
		{
		case NAI::HL_HEAD:
			fDmgModifier = 1.5f; // Retail v1.2 0x6c4092.
			break;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND:
			fDmgModifier = 0.7f;
			break;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:
			fDmgModifier = 0.85f;
			break;
		case NAI::HL_BODY:
			fDmgModifier = 1.0f;
			break;
		default:
			ASSERT(0);
			break;
		}
		if ( pAttack->atkType == AT_CLICK_OF_DEATH )
		{
			if ( !bUnconscious && !IsDead() )
			{
				pAttack->nUnconsciousProbability = 100;
				nDmg = 0;
			}
			else
				nDmg = 100000;
		}
		int nCriticalProbability = pAttack->nCrtical;
		int nCriticalDifficulty = pAttack->nCrticalDifficulty;
		// Retail v1.2 0x6c40ee..0x6c4123: roll once per attacker level
		// for this hit, before called-shot bonuses and the severity cap.
		// Get's upper bound is exclusive, so each roll contributes 1..19.
		if ( IsValid( pAttack->pAttacker ) )
		{
			for ( int i = 0; i < pAttack->pAttacker->GetSkillValue( NDb::ST_LEVEL ); ++i )
				nCriticalDifficulty += random.Get( 1, 20 );
		}
		// Retail v1.2 0x6c4139..0x6c41a4: reward a called shot only when
		// the ray hits the requested part. v1.1's head bonuses were only 50/100.
		if ( pWorld->GetGlobalGame()->pDifficulty->bHeadshotShouldKill && pAttack->eWantedHL == nUserID )
		{
			switch ( nUserID )
			{
			case NAI::HL_HEAD: nCriticalProbability += 90; nCriticalDifficulty += 150; break;
			case NAI::HL_BODY: nCriticalProbability += 10; nCriticalDifficulty += 20; break;
			case NAI::HL_RHAND:
			case NAI::HL_LHAND: nCriticalProbability += 35; nCriticalDifficulty += 65; break;
			case NAI::HL_RLEG:
			case NAI::HL_LLEG: nCriticalProbability += 25; nCriticalDifficulty += 40; break;
			}
		}
		// a critical injury may arise inside GetCriticalDmgModifier
		float fCriticalDmgModifier = 
			GetCriticalDmgModifier( pWorld, (NAI::EHitLocation)nUserID, nCriticalProbability,
				nCriticalDifficulty, pAttack->bAlwaysHumanCritical, pAttack->pAttacker );
		fDmgModifier += fCriticalDmgModifier;
		csRPG << CC_GREY << "\tHL=" << GetHLName( (NAI::EHitLocation)nUserID );
		csRPG << CC_GREY << " \tDmgModifier=" << fDmgModifier;
		int nDamage = Max( 0.f, fDmgModifier * nDmg );
		// Retail v1.2 0x6c4274..0x6c430e: only incidental victims may evade damage.
		// Intended targets, Panzerklein wearers and click-of-death skip both the roll and practice.
		if ( pAttack->atkType != AT_CLICK_OF_DEATH && this != pAttack->pTarget &&
			!GetPanzerklein() && CheckIC( pAttack->atkType ) )
			nDamage = Max( 0, nDamage - int( double( GetPerkModifiedIC( pAttack->atkType ) ) * nDamage * double( 0.01f ) ) );
		// Retail v1.2 0x6c4310..0x6c4368 clamps the converted healed share
		// to zero. A death critical can already have reduced total VP to zero:
		// retail's x87 conversion then yields INT_MIN, which that clamp discards.
		// Handle the zero divisor explicitly instead of relying on a NaN cast.
		int ndBVP = GetTotalVP() != 0 ? Max( 0, int( nDamage * (float)GetHealedVP() / GetTotalVP() ) ) : 0;
		SetHealedVP( GetHealedVP() - ndBVP );
		pRPGUnit->Skills(NDb::ST_VP).Modify( ndBVP - nDamage );
		// Acks
		if ( bAlive )
		{
			if ( IsDead() )
				SaveAck( NWorld::N_ACK_DEATH, pAttack->pAttacker );
			else if ( fCriticalDmgModifier > 0 || 
				( nDamage - ndBVP ) >= 3.f / 4.f * pRPGUnit->Skills( NDb::ST_VP ).GetMaxValue() )
				SaveAck( NWorld::N_ACK_CRITICAL, pAttack->pAttacker );
		}
		//
		csRPG << " \t" << CC_YELLOW << GetName() << CC_WHITE << " damaged on " << nDamage << "hp" << " \tPiercingAbility = " << pAttack->nK << " HP:" << pRPGUnit->Skills(NDb::ST_VP) << "\n";
		UseSkill( NDb::ST_VP );
		nTotalDmg = nDamage;
		//
		if ( !IsDead() )
		{
			int nProbability = pAttack->nUnconsciousProbability;
			int nCheck = random.Get( 1, 100 );
			// Retail v1.2 0x6c4501..0x6c45ba: scale the knockout chance by
			// remaining total health after damage; above half health it is zero.
			float fHealth = float( GetTotalVP() ) / pRPGUnit->Skills(NDb::ST_VP).GetMaxValue();
			float fKnockout = Max( 0.f, Min( 1.f, ( 0.5f - fHealth ) * 2.500000238418579f ) );
			nProbability = Float2Int( fKnockout * nProbability );
			if ( GetTotalVP() <= 0 || nCheck <= nProbability || pAttack->atkType == AT_CLICK_OF_DEATH )
			{
				bUnconscious = true;
				csRPG << " \t" << CC_YELLOW << GetName() << CC_WHITE << " made unconscious" << endl;
			}
		}
		else
			bUnconscious = false;
	}
	else
	{
		csRPG << "<font size=16pt>";
		csRPG << "<color=blue>" << " Bullet can`t penetrate target armor, BulletAPA = " << pAttack->nK << endl;
	}
	ApplyCritical( SCritical( NDb::CL_ANY, NDb::C_VP ) );
	return CReceivedDmg( nTotalDmg, RD_HUMAN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec3 Projection( const CVec3 &pt, const SPlane &plane )
{
	float t = -( pt * plane.n + plane.d ) / fabs2( plane.n );
	return pt + t * plane.n;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float IntersectionArea( const CVec3 &ptA, const CVec3 &ptB, float fRadius )
{
	const float fL = fabs( ptB - ptA );
	const float fAng = acos( Min( 1.0f, fL / (2 * fRadius) ) );
	return sqr( fRadius ) * (2 * fAng - sin( 2*fAng ));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_RADIUS = 0.25f;
float GetCubesArea( const CVec3 &ptPos, vector<CVec3> *pCubes )
{
	if ( pCubes->size() < 2 )
		return 0;
	CVec3 ptCeneter( VNULL3 );
	for ( vector<CVec3>::iterator i = pCubes->begin(); i != pCubes->end(); ++i )
		ptCeneter += *i;
	ptCeneter /= pCubes->size();
	//
	CVec3 nrm( ptCeneter - ptPos );
	Normalize( &nrm );
	SPlane plane( nrm, 0 );
	plane.RecalcDist( ptCeneter );
	for ( vector<CVec3>::iterator i = pCubes->begin(); i != pCubes->end(); ++i )
		*i = Projection( *i, plane );
	//
	float fSum = 0;
	switch ( pCubes->size() )
	{
		case 2:
			fSum += IntersectionArea( pCubes->front(), pCubes->back(), F_RADIUS );
			break;
		case 3:
			fSum += IntersectionArea( (*pCubes)[0], (*pCubes)[1], F_RADIUS );
			fSum += IntersectionArea( (*pCubes)[1], (*pCubes)[2], F_RADIUS );
			fSum += IntersectionArea( (*pCubes)[0], (*pCubes)[2], F_RADIUS );
			break;
	}
	const float fOne = FP_PI * sqr( F_RADIUS );
	return (fOne * pCubes->size() - fSum) / (fOne * 3);
}
/*
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetHLPenalty( NAI::EHitLocation hl )
{
	switch ( hl )
	{
		case NAI::HL_BODY:
			return 0.9f;
		case NAI::HL_HEAD:
			return 0.6f;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND:
			return 0.7f;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:
			return 0.8f;
	}
	return 1;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetHLPenalty( NAI::EHitLocation hl )
{
	switch ( hl )
	{
		case NAI::HL_BODY:
			return 6;
		case NAI::HL_HEAD:
			return 9;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND:
			return 8;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:
			return 7;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetVPPenalty( int nVP, int nHealedVP, int nMaxVP )
{
	const int nVPPercentage = 100.0f * ( nVP + nHealedVP / 2.f ) / nMaxVP;
	if ( nVPPercentage > 75 )
		return 1.0f;
	else if ( nVPPercentage > 50 )
		return 0.9f;
	else if ( nVPPercentage > 25 )
		return 0.75f;
	else
		return 0.5f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetToHitType @0x2b3790 -- release free-fn form of CUnitMission::GetToHitWeaponType, shared
// with the composite/cover entry points (RealCalcTileCovers @0x2b4700 takes the melee-swing cover
// path only on TH_MELEE).
EToHitType GetToHitType( const NWorld::CUnit *pAttacker )
{
	CDynamicCast<NWorld::CUnitServer> pUS( const_cast<NWorld::CUnit*>( pAttacker ) );
	if ( !pUS )
		return TH_DEFAULT;
	CUnitMission *pMission = CDynamicCast<CUnitMission>( pUS->GetUnitRPG() );
	if ( !pMission )
		return TH_DEFAULT;
	return pMission->GetToHitWeaponType();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// To-hit dispatch (release migration). Each free fn RTTI-casts the firing unit to its CUnitServer,
// picks the EToHitType from the held weapon and builds the matching ToHitCalcer (which now takes the
// CUnitServer*). Incidental bullets supply the retail night flag explicitly.
int GetToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	const NAI::SPosition &posTarget, NAI::EHitLocation hl, int nExtraAP, const NWorld::CUnit *pTarget,
	const vector<int> &accessibleHLs, int nHitCover, bool bFirstRound, const CVec3 &ptIllumination, bool bBackstab,
	int nBullet, bool bNight )   // retail @0x2b4ae0: explicit burst index and night flag
{
	CDynamicCast<NWorld::CUnitServer> pUS( const_cast<NWorld::CUnit*>( pAttacker ) );
	// retail RPGUnitGetToHit @0x2b4ae0 head gate: a script-forced to-hit (UnitSetToHit, -1 = off)
	// REPLACES the whole computation for this attacker -- shoot/melee/throw/rocket alike (grenades
	// excluded; the grenade calcer has no such read). This is how missions cap tutorial/intro
	// enemies to 8%.
	if ( pUS && pUS->GetScriptToHit() >= 0 )
		return pUS->GetScriptToHit();
	CDynamicCast<NWorld::CUnitServer> pUSTarget( const_cast<NWorld::CUnit*>( pTarget ) );
	CUnitMission *pMission = CDynamicCast<CUnitMission>( pUS->GetUnitRPG() );
	IUnitMissionInfo *pTargetRPG = pTarget ? pTarget->GetRPG() : 0;
	CPtr<IToHitCalcer> pToHitCalcer;
	switch ( pMission->GetToHitWeaponType() )
	{
		case TH_THROWING:
			pToHitCalcer = new CThrowKnifeUnitToHitCalcer( pUS, curPose, nDistance, ptAttacker, (float)nHitCover,
				bFirstRound, bNight, ptIllumination, pUSTarget, hl, posTarget, nExtraAP, bBackstab );
			break;
		case TH_MELEE:
			return pMission->GetMeleeToHit( ptAttacker, posTarget, pTargetRPG, hl, accessibleHLs, bBackstab );
		case TH_SHOOT:
			pToHitCalcer = new CUnitToHitCalcer(
				pUS, curPose, nDistance, ptAttacker,
				posTarget, nExtraAP, pMission->GetSnipeAP( pTargetRPG ), (float)nHitCover,
				bFirstRound, bNight, ptIllumination, hl, pUSTarget, nBullet, bBackstab );
			break;
		case TH_RLAUNCHER:
			pToHitCalcer = new CRLauncherToHitCalcer(
				pUS, curPose, nDistance, ptAttacker, (float)nHitCover, nExtraAP,
				bFirstRound, bNight, ptIllumination, CVec3(1,1,1) );
			break;
		default:
			ASSERT( 0 );
			return 0;
	}

	int nToHit = pToHitCalcer->GetToHit();
	if ( pMission->bLogActive )
		pToHitCalcer->Log();
	return nToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x6c0720. Preserve fractional bonuses until the calcer's final int conversion.
float CUnitMission::GetWeaponSkill( CObjectBase *pItem, int nExtraAP, int nBullet )
{
	float fSkill = GetRPGUnit()->Skills( NDb::ST_SHOOTING );
	float fBonus;
	CDynamicCast<CWeaponItem> pWeapon( pItem );
	if ( IsValid( pWeapon ) )
	{
		int nPerk = 0;
		switch ( pWeapon->GetWeaponType() )
		{
			case NDb::WT_SUB_MACHINE_GUN: nPerk = 9; break;
			case NDb::WT_MACHINE_GUN: nPerk = 13; break;
			case NDb::WT_PISTOL: nPerk = 67; break;
			case NDb::WT_RIFLE: nPerk = 68; break;
			case NDb::WT_RLAUNCHER: nPerk = 84; break;
		}
		if ( nPerk && HasPerk( nPerk, &fBonus ) )
			fSkill += fBonus;
		NDb::EShootMode mode = pWeapon->GetShootMode();
		float fMove = 0;
		// ORIGINAL RETAIL BUG FIXED (perk 19): steady hand cancels the movement
		// penalty here too, not only the additive calcer penalty.
		if ( !IsFirstTurn() && !HasPerk( 19 ) )
			fMove = Min( GetToHitConstants()->nSMaxMove, GetMoveInLastTurn() ) *
				pWeapon->GetDBWeapon()->pWeaponType->fMovePenalty;
		if ( mode == NDb::SM_Careful || mode == NDb::SM_Snipe )
			fSkill += Max( 0, int( nExtraAP + fMove ) ) * ( 1.f / 3.f );
	}
	else if ( CDynamicCast<IGrenadeItem>( pItem ) )
	{
		fSkill = GetRPGUnit()->Skills( NDb::ST_THROWING );
		if ( HasPerk( 91, &fBonus ) )
			fSkill += fBonus;
	}
	else if ( CDynamicCast<IMeleeWeaponItem> pMelee = pItem )
	{
		bool bThrowing = pMelee->GetDBMeleeWeapon()->bThrowing;
		fSkill = GetRPGUnit()->Skills( bThrowing ? NDb::ST_THROWING : NDb::ST_MELEE );
		if ( HasPerk( bThrowing ? 89 : 70, &fBonus ) )
			fSkill += fBonus;
	}
	if ( nBullet > 0 )
	{
		const float fBeforeRecoil = fSkill;
		int nExponent = nBullet;
		int nBurst = GetRPGUnit()->Skills( NDb::ST_BURST );
		float fUnstabilized = fSkill * pow( double( nBurst ) / N_MAX_SKILL, nExponent );
		// Retail truncates the stabilization percentage BEFORE multiplying by skill.
		float fStabilized = ( GetToHitConstants()->nMaxBurstStabilize * nBurst / N_MAX_SKILL ) * fSkill * 0.01f;
		int nRecoil = 0;
		if ( IsValid( pWeapon ) )
		{
			SWeaponInfo info;
			pWeapon->GetInfo( &info );
			nRecoil = info.nRecoil;
		}
		fSkill = Max( fUnstabilized, fStabilized ) * nRecoil * 0.01f;
		// ORIGINAL RETAIL BUG FIXED (perk 30): a 100% recoil reduction removes
		// the complete burst accuracy loss. Halving the decay exponent did not.
		if ( IsValid( pWeapon ) && HasPerk( 30, &fBonus ) )
			fSkill += ( fBeforeRecoil - fSkill ) * Clamp( fBonus - 1.f, 0.f, 1.f );
	}
	return fSkill * GetVPPenalty( GetRPGUnit()->Skills( NDb::ST_VP ),
		GetHealedVP(), GetRPGUnit()->Skills( NDb::ST_VP ).GetMaxValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetTileToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	CVec3 ptTilePos, NAI::ETileHitLocation eHitLocation, int nExtraAP, int nHitCover, bool bFirstRound,
	const CVec3 &ptIllumination, int nBullet )   // retail @0x2b4df0 param (see GetToHit)
{
	CDynamicCast<NWorld::CUnitServer> pUS( const_cast<NWorld::CUnit*>( pAttacker ) );
	// retail RPGUnitGetTileToHit @0x2b4df0: same script-forced to-hit head gate as the unit-target
	// dispatcher (@0x2b4ae0) -- absolute replacement, -1 = off.
	if ( pUS && pUS->GetScriptToHit() >= 0 )
		return pUS->GetScriptToHit();
	CUnitMission *pMission = CDynamicCast<CUnitMission>( pUS->GetUnitRPG() );
	const bool bNight = pUS->GetWorld()->GetGame()->IsNight();
	CPtr<IToHitCalcer> pToHitCalcer;
	switch ( pMission->GetToHitWeaponType() )
	{
		case TH_THROWING:
			// the release drops the tile knife calcer's hit-location (eHitLocation unused here).
			pToHitCalcer = new CThrowKnifeTileToHitCalcer( pUS, curPose, nDistance, ptAttacker, (float)nHitCover,
				bFirstRound, bNight, ptIllumination, ptTilePos, nExtraAP );
			break;
		case TH_MELEE:
			// retail RPGUnitGetTileToHit @0x2b4df0, TH_MELEE branch (disasm @0x6b4ecc): a melee
			// swing at a tile/object has NO calcer -- it connects with certainty whenever the
			// cover walk lets anything through (`fucompp fHitCover, 0.0f` -> equal returns 0,
			// otherwise returns 100). The previous `return 0` made every melee attack at
			// ground/walls/objects a guaranteed miss and showed a constant 0% on the cursor.
			// NOTE <= not ==: retail GetHitCover (@0x2b4390) returns exactly 0.0 for a fully
			// blocked ray set, but the dev GetHitCover encodes that case as the -1 BLOCKED
			// sentinel (consumed by CToHitCalcer::GetToHit's retail-faithful `fHitCover <= 0
			// -> 0%` gate, @0x2b85e0), which must read as a miss here too -- otherwise melee
			// through a wall would show 100%.
			return nHitCover <= 0 ? 0 : 100;
		case TH_SHOOT:
			pToHitCalcer = new CTileToHitCalcer(
				pUS, curPose, nDistance, ptAttacker, nExtraAP, (float)nHitCover, bFirstRound,
				bNight, ptIllumination, ptTilePos, nBullet );
			break;
		case TH_RLAUNCHER:
			pToHitCalcer = new CRLauncherToHitCalcer(
				pUS, curPose, nDistance, ptAttacker, (float)nHitCover, nExtraAP, bFirstRound,
				bNight, ptIllumination, ptTilePos );
			break;
		default:
			ASSERT(0);
			return 0;
	}
	int nToHit = pToHitCalcer->GetToHit();
	if ( pMission->bLogActive )
		pToHitCalcer->Log();
	return nToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetRLauncherToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	CVec3 ptTilePos, NAI::ETileHitLocation eHitLocation, int nExtraAP, bool bFirstRound, const CVec3 &ptIllumination )
{
	CDynamicCast<NWorld::CUnitServer> pUS( const_cast<NWorld::CUnit*>( pAttacker ) );
	CUnitMission *pMission = CDynamicCast<CUnitMission>( pUS->GetUnitRPG() );
	CPtr<CRLauncherToHitCalcer> pToHitCalcer =
		new CRLauncherToHitCalcer( pUS, curPose, nDistance, ptAttacker, 100.f, nExtraAP, bFirstRound,
			pUS->GetWorld()->GetGame()->IsNight(), ptIllumination, ptTilePos );

	int nToHit = pToHitCalcer->GetToHit();

	if ( pMission->bLogActive )
		pToHitCalcer->Log();

	return nToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CGame::GetObjectToHit / CObjectToHitCalcer dropped in the release.)
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetGrenadeToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	bool bFirstRound, CVec3 ptTilePos, const CVec3 &ptIllumination )
{
	CDynamicCast<NWorld::CUnitServer> pUS( const_cast<NWorld::CUnit*>( pAttacker ) );
	CUnitMission *pMission = CDynamicCast<CUnitMission>( pUS->GetUnitRPG() );
	// release: the thrown grenade is no longer an argument -- the calcer derives it from the active item.
	CPtr<CGrenadeToHitCalcer> pToHitCalcer =
		new CGrenadeToHitCalcer( pUS, curPose, nDistance, ptAttacker,
			bFirstRound, pUS->GetWorld()->GetGame()->IsNight(), ptIllumination, ptTilePos );

	int nToHit = pToHitCalcer->GetToHit();

	if ( pMission->bLogActive )
		pToHitCalcer->Log();

	return nToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetBulletsQuantityInShot() const
{
	EToHitType type = GetToHitWeaponType();
	if ( type == TH_MELEE || type == TH_THROWING )
		return 1;
  CWeaponItem *pW = pRPGUnit->GetWeaponItem();
	if ( !pW )
	{
		ASSERT(0);
		return 1;
	}
	NDb::CRPGWeapon *pDBW = pW->GetDBWeapon();
	if ( !pDBW )
	{
		ASSERT(0);
		return 1;
	}
	switch ( pW->GetShootMode() )
	{
		case NDb::SM_Snap:
		case NDb::SM_Aimed:
		case NDb::SM_Careful:
		case NDb::SM_Snipe:
			return 1;
		case NDb::SM_ShortBurst:
		case NDb::SM_LongBurst:
			return pDBW->nRoF / 6;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static float GetAccessibleHLsPenalty( const vector<int> &hls )
{
	static const int TOTAL_VISIBLE = 5 + 10 + 2 * 5 + 2 * 7;
	int nVisible = 0;
	for ( int i = 0; i < hls.size(); ++i )
		switch ( hls[i] )
		{
			case NAI::HL_HEAD:
				nVisible += 5;
				break;
			case NAI::HL_BODY:
				nVisible += 10;
				break;
			case NAI::HL_RHAND:
			case NAI::HL_LHAND:
				nVisible += 5;
				break;
			case NAI::HL_RLEG:
			case NAI::HL_LLEG:
				nVisible += 7;
				break;
		}
	int nInvisible = TOTAL_VISIBLE - nVisible;
	return 0.01f * (100 - nInvisible);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::RPGUnitGetMeleeToHit @0x2b3990 (disasm-verified) -- the release melee to-hit:
// base 70 + 0.5*(skill diff), the TARGET's held-weapon fMeleePenalty softens his defence skill,
// backstab is a flat x1.5 on the FINAL value (not the old F_BACKSTAB_MELEE_COEFF on the skill),
// the called-shot weighting is gated on the difficulty's bHeadshotShouldKill, and the result is
// clamped to [2, 100].
int CUnitMission::GetMeleeToHit( const CVec3 &ptAttacker, const NAI::SPosition &posTarget,
	IUnitMissionInfo *pTarget, NAI::EHitLocation hl, const vector<int> &accessibleHLs, bool bBackStab ) const
{
	CMeleeWeaponItem *pW = pRPGUnit->GetMeleeWeaponItem();
	if ( !pTarget || !pW )
		return 0;
	NDb::CRPGMeleeWeapon *pDBW = pW->GetDBMeleeWeapon();
	if ( !pDBW )
		return 0;
	const int nSkill = GetSkillValue( NDb::ST_MELEE );
	int nTargetSkill = pTarget->GetSkillValue( NDb::ST_MELEE );
	// retail @0x6b39f8..0x6b3a63: the target's held item hampers his melee defence -- his weapon
	// type's fMeleePenalty scales the skill down (a melee weapon in hand wins over a gun).
	if ( CUnit *pTargetUnit = pTarget->GetRPGUnit() )
	{
		NDb::CRPGWeaponType *pTargetType = 0;
		if ( CWeaponItem *pTW = pTargetUnit->GetWeaponItem() )
			pTargetType = pTW->GetDBWeapon()->pWeaponType;
		if ( CMeleeWeaponItem *pTMW = pTargetUnit->GetMeleeWeaponItem() )
			pTargetType = pTMW->GetDBMeleeWeapon()->pWeaponType;
		if ( pTargetType )
			nTargetSkill = (int)( ( 1.f - pTargetType->fMeleePenalty ) * nTargetSkill );
	}
	const float nWBonus = pDBW->nToHitBonus;
	const float fMovePenalty = nMoveInLastTurn * pDBW->pWeaponType->fMovePenalty;
	const float fVPPenalty = GetVPPenalty( pRPGUnit->Skills(NDb::ST_VP), pRPGUnit->nHealedVP, pRPGUnit->Skills(NDb::ST_VP).GetMaxValue() );
	const float fAccessibleHLsPenalty = GetAccessibleHLsPenalty( accessibleHLs );
	//
	float fToHit = ( 0.5f * ( nSkill - nTargetSkill ) + fMovePenalty + nWBonus + 70.f )
		* fAccessibleHLsPenalty * fVPPenalty;
	if ( bBackStab )
		fToHit *= 1.5f;
	// retail @0x6b3ae9: the called-shot weighting, live only when the difficulty says headshots kill
	if ( IsValid( pGlobalGame ) && IsValid( pGlobalGame->pDifficulty ) && pGlobalGame->pDifficulty->bHeadshotShouldKill )
		fToHit = GetHeadshotMultiplier( hl ) * fToHit;
	fToHit = Max( 2.0f, Min( fToHit, 100.0f ) );
	if ( bLogActive )
	{
		csRPG << "<font size=16pt>";
		csRPG << CC_GREEN << " \tToHit: ";
		csRPG << CC_ORANGE << " \tSkill=" << CC_GREY << nSkill;
		csRPG << CC_ORANGE << " \tbBackStab=" << CC_GREY << bBackStab;
		csRPG << CC_ORANGE << " \tTarget skill=" << CC_GREY << nTargetSkill;
		csRPG << CC_ORANGE << " \tMove bonus=" << CC_GREY << (int)fMovePenalty;
		csRPG << CC_ORANGE << " \tWeapon bonus=" << CC_GREY << (int)nWBonus;
		csRPG << CC_ORANGE << " \tHL penalty=" << CC_GREY << int( 100 * (1.0f-fAccessibleHLsPenalty) ) << "%";
		csRPG << CC_ORANGE << " \tVPPenalty=" << CC_GREY << int(100 * (1.0f - fVPPenalty)) << "%";
		csRPG << CC_GREEN << " \tToHit=" << CC_GREY << fToHit << endl;
	}
	return fToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetIC() const
{
	float fIC = pRPGUnit->Skills( NDb::ST_IC );
	float fParam;
	if ( HasPerk( N_PERK_BETTER_EVASION, &fParam ) )
		fIC = ( 1.0 + double( fParam ) ) * fIC;
	return Float2Int( fIC ); // Retail 0x6bff40 rounds the stored float, rather than truncating.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetPerkModifiedIC( EAttackType attackType ) const
{
	float fIC = GetIC();
	float fParam;
	if ( ( attackType == AT_BLAST_WAVE || attackType == AT_FRAGMENT ) &&
		HasPerk( N_PERK_EXPLOSION_EVASION, &fParam ) )
		fIC = ( 1.0 + double( fParam ) ) * fIC;
	fIC += fAuraPerkICModifier;
	return Float2Int( fIC ); // Retail 0x6becf0.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::CheckIC( EAttackType attackType )
{
	UseSkill(NDb::ST_IC);
	bool isCheck = random.Check( GetPerkModifiedIC( attackType ) );
	csRPG << "\t" << GetName() << " Dodge:" << isCheck << endl;
	return isCheck;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::GetWeaponAdaptation() const
{
	// Retail 0x6c0cf0 reads the live CUnit counter, not the mission's serialized mirror.
	IWeaponItem *pWeapon = pRPGUnit->GetWeaponItem();
	IMeleeWeaponItem *pMelee = pRPGUnit->GetMeleeWeaponItem();
	if ( IsValid( pWeapon ) )
		return pRPGUnit->GetWeaponAdaptation( pWeapon );
	if ( IsValid( pMelee ) )
		return pRPGUnit->GetWeaponAdaptation( pMelee );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::CalcInterruptProbability( const IUnitMission *pEnemy,
	bool bIsMutual, bool bWasShot )
{
	int nRes = 0; // interrupt probability
	NDb::SInterruptsConstants *pConst = GetInterruptsConstants();

	SUnitInfo sUnitInfo;
	GetInfo( NAI::WALK, &sUnitInfo );
	if ( sUnitInfo.nAP <= pConst->nMinInterruptAP )
		return nRes;

	int nASkill = pRPGUnit->Skills(NDb::ST_INTERRUPT); 	// A - who
	int nBSkill = pEnemy->GetRPGUnit()->Skills(NDb::ST_INTERRUPT); 	// B - whom

	if ( bWasShot )
	{
		nRes = pConst->nMissedShotInterruptsBase + ( nASkill - nBSkill );
	} 
	else
	{
		if ( bIsMutual )
		{
			nRes = pConst->nInterruptsBase + ( nASkill - nBSkill );
		}
		else
		{
			nRes = pConst->nBackInterruptsBase + ( nASkill - nBSkill );
		}
	}

	// reduce the probability for spent AP
	int nAPPenalty = 0;
	nAPPenalty = pConst->fAPInterruptReduction * ( sUnitInfo.nMaxAP - sUnitInfo.nAP );

	// compute the result
	nRes -= nAPPenalty;
	nRes = Clamp( nRes, 5, 95 );

	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::CheckInterrupt( const IUnitMission *pEnemy, bool bIsMutual, bool bWasShot )
{
	int nProbability = CalcInterruptProbability( pEnemy, bIsMutual, bWasShot );
	int nCheck = random.Get(100);
	csRPG << CC_YELLOW << GetName() << CC_WHITE << " interrupt "<< CC_YELLOW << pEnemy->GetName() << CC_WHITE
		  << " probability " << nProbability << "% Check:" << nCheck << endl;
	UseSkill( NDb::ST_INTERRUPT );
	return nProbability - nCheck;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::Kill()
{
	bUnconscious = false;
	pRPGUnit->Kill();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::RemoveCritical( NDb::ECritical eCritical )
{
	for ( vector<CObj<CCritical> >::iterator i = criticals.begin(); i != criticals.end(); )
	{
		if ( (*i)->GetCritical().eCritical == eCritical )
		{
			i = criticals.erase( i ); // remove only one critical of this type
			return true;
		}
		else
			++i;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::ApplyCritical( CCritical *p )
{
	if ( !IsValid( p ) )
		return;
	// Retail v1.2 0x6c3368..0x6c3373: a new bleeding injury cancels
	// temporary suppression BEFORE merging, even if the injury is weaker.
	if ( p->GetCriticalType() == NDb::C_BLEEDING )
		nBleedingStopAmount = 0;

	for ( vector<CObj<CCritical> >::iterator i = criticals.begin(); i != criticals.end(); ++i )
	{
		if ( !(*i)->CanBeMerged() )
			continue;
		switch( (*i)->Merge( p ) )
		{
		case CCritical::WEAKER:
			return;
		case CCritical::MERGED:
			*i = p;
			if ( !IsValid( *i ) || !(*i)->SetModifiers( pRPGUnit, this ) )
				criticals.erase( i );
			return;
		}
	}
	if ( IsValid( p ) && p->SetModifiers( pRPGUnit, this ) )
		criticals.push_back( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::ApplyCritical( const SCritical &cr )
{
	CPtr<CCritical> p = CreateCritical( cr );
	ApplyCritical( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::ApplyCritical( NDb::CRPGCritical *p, int nDC )
{
	// Combat severity is capped before selecting the critical, not after it.
	int nDuration = -1;
	if ( p->nMinDuration > 0 )
		nDuration = p->nMinDuration + random.Get( p->nMaxDuration - p->nMinDuration );
	csRPG << "<font size=16pt>";
	csRPG << "<color=red>" << "\tCritical: " << "<color=yellow>" << "\"" << p->szName << "\", difficulty=" << nDC;
	csRPG << "  Duration=" << nDuration << "(" << p->nMinDuration << " : " << p->nMaxDuration;
	csRPG << ")  HL=" << GetCLName( p->hl ) << "\n";
	ApplyCritical( SCritical( p->hl, p->type, nDuration, p->fValue, nDC ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SuspendCriticals( int nTurns )
{
	ASSERT( nTurns > 0 );
	if ( nTurns <= 0 )
		return;
	SCriticalsHolder &h = *suspendedCriticals.insert( suspendedCriticals.end(), SCriticalsHolder());
	h.nTimeLeft = nTurns;
	for ( vector<CObj<CCritical> >::iterator i = criticals.begin(); i != criticals.end();  )
	{
		CCritical *p = *i;
		if ( p->CanBeSuspended() )
		{
			h.criticals.push_back( p );
			p->RemoveModifiers();
			i = criticals.erase( i );
		}
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SegmentCriticals( vector<CObj<CCritical> > *pRes )
{
	for ( vector<CObj<CCritical> >::iterator i = pRes->begin(); i != pRes->end();  )
	{
		if ( !(*i)->NextTurn() )
			i = pRes->erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitMission::SegmentCriticals @0x2c46a0: tick active criticals, tick/expire the
// suspended-criticals holders, then the retail addition -- tick/expire the POSTPONED skill
// modifiers, rebuilding a live CSkillModifier from each expired holder into temporaryModifiers.
void CUnitMission::ProcessCriticalsOnNewTurnFor()
{
	SegmentCriticals( &criticals );
	for ( list<SCriticalsHolder>::iterator i = suspendedCriticals.begin(); i != suspendedCriticals.end(); )
	{
		if ( --i->nTimeLeft > 0 )
		{
			SegmentCriticals( &i->criticals );
			++i;
		}
		else
		{
			for ( int k = 0; k < i->criticals.size(); ++k )
				ApplyCritical( i->criticals[k] );
			i = suspendedCriticals.erase( i );
		}
	}
	for ( list<SModifierHolder>::iterator m = postponedModifiers.begin(); m != postponedModifiers.end(); )
	{
		if ( --m->nTimeLeft > 0 )
			++m;
		else
		{
			if ( IsValid( m->pTarget ) )
				temporaryModifiers.push_back( new CSkillModifier( m->pTarget, m->info ) );
			m = postponedModifiers.erase( m );   // releases m->pTarget
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetMaxExtraAP() const
{
	CWeaponItem *pW = pRPGUnit->GetWeaponItem();
	if ( !pW )
		return 0;
	SWeaponInfo info;
	pW->GetInfo( &info );
	return info.nTargetingAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NDb::CRPGCritical* GetCritical( int nProbability, 
	const vector<SCriticalType> &criticals, const vector<ECriticalState> &criticalsStates )
{
	vector<SCriticalType> enabledCriticals;
	for ( vector<SCriticalType>::const_iterator i = criticals.begin(); i != criticals.end(); ++i )
		if ( criticalsStates[ (*i).pCritical->type ] == CS_ENABLED )
			enabledCriticals.push_back( *i );
	//
	if ( enabledCriticals.empty() )
		return 0;
	//
	if ( nProbability >= enabledCriticals.back().nStartPos )
		return enabledCriticals.back().pCritical;
	//
	for ( int i = 1; i < enabledCriticals.size(); ++i )
		if ( nProbability < enabledCriticals[i].nStartPos )
			return enabledCriticals[i-1].pCritical;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::RollCritical( NAI::EHitLocation eHL, int nCriticalDifficulty, NDb::CRPGCritical **pCritical )
{
	int nRezDiff = nCriticalDifficulty;
	switch ( eHL )
	{
		case NAI::HL_HEAD:
			*pCritical = GetCritical( nRezDiff, criticalBar[NDb::CL_HEAD], criticalsState );
			break;
		case NAI::HL_BODY:
			*pCritical = GetCritical( nRezDiff, criticalBar[NDb::CL_TORSO], criticalsState );
			break;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND:
			*pCritical = GetCritical( nRezDiff, criticalBar[NDb::CL_ARMS], criticalsState );
			break;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:
			*pCritical = GetCritical( nRezDiff, criticalBar[NDb::CL_LEGS], criticalsState );
			break;
	}
	return nRezDiff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::GetCriticalDmgModifier( NWorld::IWorld *pWorld, NAI::EHitLocation eHL,
	int nCriticalProbability, int nCriticalDifficulty, bool bForced, IUnitMissionInfo *pAttacker )
{
	if ( nCriticalProbability == 0 )
		return 0;
	static SRand rand;
	if ( !bCBarInitialized )
	{
		InitializeCriticals();
		bCBarInitialized = true;
	}
	const int nRoll = rand.Get( 100 );
	float fCriticalResist = pPanzerklein ? 1.f - pPanzerklein->fCriticalResist : 1.f;
	float fParam = 0;
	if ( HasPerk( N_PERK_CRITICAL_RESISTANCE, &fParam ) )
		fCriticalResist *= 1.f - fParam;
	// ORIGINAL RETAIL BUG FIXED (perk 47): its 25% critical-damage reduction
	// applies to the resulting extra damage, not to the injury-selection input.
	// Dividing severity by Param1=1.25 gave 20% and no fixed HP reduction.
	float fCriticalDamageMultiplier = 1.0f;
	if ( HasPerk( N_PERK_REDUCED_CRITICAL_SEVERITY, &fParam ) )
		fCriticalDamageMultiplier = 2.0f - fParam;
	if ( IsValid( pGame ) )
		nCriticalDifficulty = Min( nCriticalDifficulty, pGame->GetMaxCriticalSeverity() );

	// Retail v1.2 0x6c3ab9: the difficulty option supplies a level-based
	// fallback only for an unresisted, otherwise failed probability roll.
	if ( !bForced && nRoll > nCriticalProbability * fCriticalResist )
	{
		if ( !pWorld->GetGlobalGame()->pDifficulty->bAlwaysCritical || fCriticalResist < 1.f )
			return 0;
		if ( IsValid( pAttacker ) )
		{
			nCriticalDifficulty = 0;
			for ( int i = 0; i < pAttacker->GetSkillValue( NDb::ST_LEVEL ); ++i )
				nCriticalDifficulty += random.Get( 1, 5 );
		}
	}
	NDb::CRPGCritical *pCritical = 0;
	const int nSeverity = RollCritical( eHL, nCriticalDifficulty, &pCritical );
	if ( !IsValid( pCritical ) )
		return 0;
	float fDamage = nSeverity / 300.f;
	switch ( eHL )
	{
	case NAI::HL_HEAD: fDamage *= 2.f; break;
	case NAI::HL_BODY: fDamage *= 1.5f; break;
	case NAI::HL_RHAND:
	case NAI::HL_LHAND: fDamage *= 0.5f; break;
	}
	ApplyCritical( pCritical, nSeverity );
	return fDamage * fCriticalResist * fCriticalDamageMultiplier;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::DumpStats() const
{
	csRPG << "\t " << GetName() << endl;
	NRPG::DumpStats( pRPGUnit, GetHealedVP() );
	csRPG << "\n";
	for ( int i = 0; i < criticals.size(); ++i )
		DumpCritical( criticals[i] );
	csRPG << "\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SaveAP( const SSnipeAP &ap )
{
	savedSnipeAP.pTarget = ap.pTarget;
	savedSnipeAP.nAP = ap.nAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::HealVP( const SFirstAid &fa )
{
	float fHealed = GetRPGUnit()->Heal( fa );
	ApplyCritical( SCritical( NDb::CL_ANY, NDb::C_VP ) );
	return fHealed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::HealCriticals( int nDC, bool bStopBleeding )
{
	float fHealed = 0;
	for ( vector<CObj<CCritical> >::iterator i = criticals.begin(); i != criticals.end(); )
	{
		const SCritical &c = (*i)->GetCritical();
		if ( ( c.nDC <= nDC && (*i)->CanBeSuspended() ) ||
			( bStopBleeding && c.eCritical == NDb::C_BLEEDING ) )
		{
			csRPG << "\tCured: \t";
			DumpCritical( *i );
			fHealed += c.nDC;
			i = criticals.erase( i );
		}
		else
			++i;
	}
	ApplyCritical( SCritical( NDb::CL_ANY, NDb::C_VP ) );
	return fHealed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHealCriticalInfo CUnitMission::TryHealCritical( int nDC, bool bStopBleeding ) const
{
	for ( int i = 0; i < criticals.size(); ++i )
	{
		const SCritical &c = criticals[i]->GetCritical();
		if ( ( c.nDC <= nDC && criticals[i]->CanBeSuspended() ) ||
			( bStopBleeding && c.eCritical == NDb::C_BLEEDING ) )
		{
			SHealCriticalInfo info = { c.eCritical, c.eCl, c.nDC };
			return info;
		}
	}
	SHealCriticalInfo info = { NDb::C_NONE, NDb::CL_ANY, 0 };
	return info;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitMission::HealCritical( const SHealCriticalInfo &info )
{
	float fHealed = 0;
	for ( vector<CObj<CCritical> >::iterator i = criticals.begin(); i != criticals.end(); )
	{
		const SCritical &c = (*i)->GetCritical();
		if ( c.eCritical == info.eCritical && c.eCl == info.eCl &&
			( (*i)->CanBeSuspended() || c.eCritical == NDb::C_BLEEDING ) )
		{
			fHealed += c.nDC;
			i = criticals.erase( i );
		}
		else
			++i;
	}
	ApplyCritical( SCritical( NDb::CL_ANY, NDb::C_VP ) );
	return fHealed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::AddPostponedModifier( CDynamicSkill *pSkill, const SSkillModifyInfo &info, int nTurns )
{
	SModifierHolder h;
	h.pTarget = pSkill;
	h.info = info;
	h.nTimeLeft = nTurns;
	postponedModifiers.push_back( h );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::Reload()
{
	CWeaponItem *pW = pRPGUnit->GetWeaponItem();
	if ( pW )
		pW->Reload( pRPGUnit->GetInventory() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::LoadWeapon( IWeaponItemInfo *pWeapon, IClipItem *pClip )
{
	CDynamicCast<CWeaponItem> pWeaponItem( pWeapon );
	if ( !IsValid( pWeaponItem ) )
		return false;

	if ( !pWeaponItem->CanLoad( pClip ) )
		return false;

	if ( pWeaponItem->HasAmmo() && !pWeaponItem->Unload( pRPGUnit->GetInventory() ) )
		return false;

	return pWeaponItem->Load( pClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::UnloadWeapon( IWeaponItemInfo *pWeapon )
{
	CDynamicCast<CWeaponItem> pWeaponItem( pWeapon );
	if ( !IsValid( pWeaponItem ) )
		return false;

	return pWeaponItem->Unload( pRPGUnit->GetInventory() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SetCannonItem( IWeaponItem *pItem )
{
	pRPGUnit->SetCannonItem( dynamic_cast<CWeaponItem*>(pItem) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2c2ee0 -- the Jan03 in-place hearing model was replaced by a roll of the hearing chance
// (GetHearingProbability @0x2bff30, which owns the deaf/silencer/perk handling) at the 1.6x-scaled 3D
// distance (0x3fcccccd), against a d99 (Isaac % 99). Retail v1.2 0x6c2ff0 trains
// SPOT only on successful quiet-sound detection, not guaranteed loud-sound hearing.
bool CUnitMission::CanHearSound( const CVec3 &ptSoundPosition, const CVec3 &ptListenerPosition,
		const NDb::SAISound &sound, IUnitMission *pSource )
{
	float fDistance = fabs( ptSoundPosition - ptListenerPosition ) * 1.6f;
	bool bQuietSound;
	if ( GetHearingProbability( pSource, fDistance, sound, &bQuietSound ) <= (int)random.Get( 99 ) )
		return false;
	if ( bQuietSound )
		UseSkill( NDb::ST_SPOT );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitMission::GetHearingProbability @0x2bff30 -- the probability side of CanHearSound above: it reports HOW
// LIKELY this unit is to hear `sound` coming from pSource at scaled distance fDist (CanHearSound rolls it, the
// assassin reaction thresholds it `>= 15`). `this` is the LISTENER; pSource is the unit being heard. The
// silencer scales the sound radius; two data-driven perks modulate the spot skill (id 0x14) and the loud-sound
// hearing distance (id 0x4c).
//
// (Workflow corrections to the answer-key/decomp labels, resolved from the raw vtable: there is NO difficulty
// global -- the "_DAT" chain is an inlined Skills(ST_SPOT/ST_STEALTH) CDynamicSkill read; the "ptLastCP" loud-
// branch read is actually pPanzerklein->fSensorRange. Perk ids 0x14/0x4c are retail data-driven ids, emitted as
// literals. Retail v1.2 truncates both perk-adjusted SPOT and the clamped probability.)
int CUnitMission::GetHearingProbability( IUnitMission *pSource, float fDist, const NDb::SAISound &sound, bool *pAudible )
{
	if ( pAudible )
		*pAudible = false;
	if ( HasCritical( NDb::C_DEAF ) )
		return 0;
	//
	float fRadius = sound.pAISound->GetRadiusFromAISoundType( sound.nSoundType ) * sound.fSilencer;
	NDb::SAISoundConstants *c = GetAISoundConstants();
	// the listener's SPOT skill, optionally boosted by the perception perk (id 0x14)
	int nSpot = GetRPGUnit()->Skills( NDb::ST_SPOT );
	float fPerk;
	if ( HasPerk( 0x14, &fPerk ) )
		nSpot = (int)( nSpot * fPerk );
	//
	if ( !pSource || fRadius >= c->nLoudSound )
	{
		// a loud sound -> a hard hearing-DISTANCE test: certainly heard (100) if within range, else 0
		float fHearingDistance = c->nPrecisePositionRadius +
			( fRadius - c->nPrecisePositionRadius ) * nSpot / NRPG::N_MAX_SKILL;
		if ( pPanzerklein && pPanzerklein->fSensorRange )
		{
			fHearingDistance *= pPanzerklein->fSensorRange;
			// Retail 0x6c029a..bf: the sensor perk applies only inside this PK branch.
			if ( HasPerk( 0x4c, &fPerk ) )
				fHearingDistance *= fPerk;
		}
		return ( fHearingDistance >= fDist ) ? 100 : 0;   // pAudible stays false on this branch
	}
	else
	{
		// a soft sound -> the PROBABILITY the listener hears it (the assassin's hear-chance answer)
		int nStealth = pSource->GetRPGUnit()->Skills( NDb::ST_STEALTH );
		float fHearingProbability = c->nBaseProbability +
			( nSpot + ( c->nPrecisePositionRadius - fDist ) * c->nDistanceCoeff - nStealth ) * 100 / NRPG::N_MAX_SKILL;
		if ( pSource->IsHiding() )
			fHearingProbability *= c->fHideCoeff;
		if ( pAudible )
			*pAudible = true;
		return Clamp( fHearingProbability, 0.0f, 95.0f );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* CUnitMission::GetRPGPers() const
{
	return pRPGUnit->GetPers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CComplexHead* CUnitMission::GetRPGPersHead() const
{
	if ( IsValid( pRPGUnit->pPers->pPanzerklein ) )
		return 0;

	return pRPGUnit->GetHead();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::HasCurableCriticals() const
{
	// Retail v1.2 0x6bf710: a curable critical, not merely any active status.
	for ( vector<CObj<CCritical> >::const_iterator i = criticals.begin(); i != criticals.end(); ++i )
		if ( (*i)->CanBeSuspended() )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::HasCritical( NDb::ECritical eCritical, CCritical** ppCritical ) const
{
	for ( vector<CObj<CCritical> >::const_iterator i = criticals.begin(); i != criticals.end(); ++i )
		if ( (*i)->GetCriticalType() == eCritical )
		{
			if ( ppCritical != 0 )
				*ppCritical = *i;
			return true;
		}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::EnableCriticals()
{
	for ( int i = 0; i < NDb::N_CRIT_TYPES; ++i )
		criticalsState[i] = CS_ENABLED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::DisableCriticals()
{
	for ( int i = 0; i < NDb::N_CRIT_TYPES; ++i )
		criticalsState[i] = CS_DISABLED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::DisableCritical( NDb::ECritical eC, ECriticalState eState  )
{
	criticalsState[eC] = eState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::GetCriticalsList( list<CPtr<ICriticalInfo> > *pListCriticals ) const
{
	for ( vector<CObj<CCritical> >::const_iterator i = criticals.begin(); i != criticals.end(); ++i )
		pListCriticals->push_back( i->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2c3b00: the Jan03 Skills().Modify(delta) flow became explicit CSkillModifier objects
// in the pkModifiers vector (clearing it drops the OLD suit's modifiers -- retail does NOT
// subtract the old PK's changes). A PK with no VP left wears broken (retail tests < 1, not < 0);
// perk 0x1d lets the wearer IGNORE negative skill penalties from the suit.
void CUnitMission::SetPanzerklein( NDb::CPanzerklein *pPK, CDynamicSkill *_pPanzerkleinVP, IInventory *_pPKInventory )
{
	pPanzerkleinVP = _pPanzerkleinVP;
	if ( pPanzerkleinVP && ( *pPanzerkleinVP < 1 ) )
		WearBrokenPK();
	pkModifiers.clear();   // each CObj releases -> the skill Update() sweeps the dead weak refs
	bool bAdaptPerk = HasPerk( 0x1d );
	for ( int skill = NDb::ST_MELEE; skill < NDb::SKILL_TYPE_NUMBERS; ++skill )
	{
		if ( skill == NDb::ST_VP )
			continue;
		int nChange = 0;
		if ( pPK && pPK->pChangeValues )
			nChange = pPK->pChangeValues->skills[ skill ];
		if ( ( !bAdaptPerk || nChange >= 0 ) && nChange != 0 )
			pkModifiers.push_back( new CSkillModifier( &pRPGUnit->Skills( skill ), SSkillModifyInfo( 1.0f, float( nChange ) ) ) );
	}
	pPanzerklein = pPK;
	GetInventory()->SetPanzerklein( pPK, _pPKInventory );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2c4f50 tail: a unit whose pers IS a Panzerklein wears itself -- SetPanzerklein over its
// own VP skill, then a modifier lifts VP to the PK record's nMaxVP (AddModifier bumps the live value).
void CUnitMission::InitAsPanzerklein( NDb::CPanzerklein *pPK )
{
	CDynamicSkill &vp = pRPGUnit->Skills( NDb::ST_VP );
	SetPanzerklein( pPK, &vp, GetInventory() );
	int nModif = pPK->nMaxVP - (int)vp;
	if ( nModif != 0 )
		pkModifiers.push_back( new CSkillModifier( &vp, SSkillModifyInfo( 1.0f, float( nModif ) ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2c3400: a VP drug boost -- fAdd = (VP base) * strength * 1%, installed as a temporary
// modifier with a recorded lifetime, followed by the overdose gate.
void CUnitMission::AddVPBoost( float fStrength, int nDuration )
{
	CDynamicSkill &vp = pRPGUnit->Skills( NDb::ST_VP );
	SSkillModifyInfo info( 1.0f, float( vp.GetTheoreticalMax() ) * fStrength * 0.01f );
	temporaryModifiers.push_back( new CSkillModifier( &vp, info ) );
	vpBoostDurations.push_back( nDuration );
	CheckOverdose();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::CheckOverdose()
{
	if ( vpBoostDurations.empty() )
		return;
	// Retail 0x6c0c00 computes sum(1 / duration) without division. Zero duration
	// also triggers; keep that explicit rather than dividing by zero.
	double fNumerator = 0, fDenominator = 1;
	for ( int i = 0; i < vpBoostDurations.size(); ++i )
	{
		fNumerator = vpBoostDurations[i] * fNumerator + fDenominator;
		fDenominator *= vpBoostDurations[i];
	}
	if ( fNumerator > fDenominator || fDenominator == 0 )
	{
		vpBoostDurations.clear();
		if ( !IsDead() )
		{
			bUnconscious = true;
			pRPGUnit->Skills( NDb::ST_VP ).SetValue( 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CalcBleedingDamage( int nVP, int nMaxVP, float fCritical, int nStopped )
{
	if ( nVP <= 0 )
		return 0;
	int nDamage = int( fCritical );
	int nHalfVP = Float2Int( nMaxVP * 0.5f );
	if ( nVP < nHalfVP )
		nDamage = int( float(nDamage) + ( nHalfVP - nVP ) * 0.1f );
	return Max( 0, nDamage - nStopped );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::DoRegenerations( NWorld::IWorld *pWorld, int *pnBleed )
{
	for ( int i = 0; i < vpBoostDurations.size(); ++i )
		--vpBoostDurations[i];
	CheckOverdose();
	// Retail v1.2 0x6c49f0: critical lifetimes advance at end of turn / real-time tick,
	// not when AP is refreshed. Compute whole-number bleeding before PK regeneration.
	ProcessCriticalsOnNewTurnFor();
	CCritical *pBleeding = 0;
	HasCritical( NDb::C_BLEEDING, &pBleeding );
	CDynamicSkill &vp = pRPGUnit->Skills( NDb::ST_VP );
	*pnBleed = CalcBleedingDamage( int(vp) + Max( 0, GetHealedVP() ), vp.GetTheoreticalMax(),
		pBleeding ? pBleeding->GetValue() : 0, nBleedingStopAmount );
	ApplyCritical( SCritical( NDb::CL_ANY, NDb::C_VP ) );
	if ( !pPanzerklein )
		return;
	if ( !pPanzerklein->fRegenerationValue )
		return;
	SFirstAid fa;
	fa.fdVP = pPanzerklein->fRegenerationValue;
	fa.nMaxVP = N_MAX_VP;
	HealVP( fa );
	HealCriticals( N_MAX_DC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::IsHero() const
{
	return pRPGUnit->IsHero();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetSkillAddValue( const int eSkill ) const
{
	if ( !pPanzerklein )
		return 0;
	if ( !pPanzerklein->pChangeValues )
		return 0;
	if ( eSkill == NDb::ST_VP )
		return 0;
	return pPanzerklein->pChangeValues->skills[ eSkill ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMission::SetHiding( bool _bHiding )
{ 
	bHiding = _bHiding; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetGrenadeTrapDC( NDb::CRPGGrenade *pGrenade )
{
	int nEngineering = GetRPGUnit()->Skills( NDb::ST_ENGINEERING );
	float fParam;
	if ( HasPerk( N_PERK_DIFFICULT_TRAPS, &fParam ) )
		nEngineering = int( ( 1.0 + double( fParam ) ) * nEngineering );
	return nEngineering;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitMission::GetMineDC( NDb::CRPGMine *pMine )
{
	int nEngineering = GetRPGUnit()->Skills( NDb::ST_ENGINEERING );
	float fParam;
	if ( HasPerk( N_PERK_DIFFICULT_TRAPS, &fParam ) )
		nEngineering = int( ( 1.0 + double( fParam ) ) * nEngineering );
	return nEngineering;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitMission::GetMineSpotRange @0x2c0340 (disasm-verified): the Jan03 CanSeeMine formula
// hoisted into its own virtual, plus a perk adjustment Jan03 lacked --
//   eng  = Skills(ST_ENGINEERING)                                  (effective value, skills[8])
//   spot = Skills(ST_SPOT)                                         (skills[6])
//   if ( HasPerk( 0x50, &fPerk ) ) spot = (int)( ( fPerk + 1.0f ) * spot )   // @0x6c03b2:
//        fld perkOut; fadd 1.0 (imm @0x8b1a24); fimul spotInt; fistp with RC=11 (TRUNCATE)
//   return Max( Max( spot - nSpotSkillModif, eng ) - nMinerEngineerSkillModif - nDC, 0 )
//          / (float)nMineSpotModif                                 (fild/fidiv @0x6c0409)
float CUnitMission::GetMineSpotRange( int nDC )
{
	int nEngSkill = GetRPGUnit()->Skills( NDb::ST_ENGINEERING );
	int nSpotSkill = GetRPGUnit()->Skills( NDb::ST_SPOT );
	float fPerk = 0;
	if ( HasPerk( 0x50, &fPerk ) )
		nSpotSkill = (int)( ( fPerk + 1.0f ) * nSpotSkill );
	int nSkill = Max( nSpotSkill - pMinesConstants->nSpotSkillModif, nEngSkill );
	return Max( nSkill - nDC - pMinesConstants->nMinerEngineerSkillModif, 0 ) * 1.f / pMinesConstants->nMineSpotModif;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitMission::CanSeeMine @0x2bec30: a pure delegate through the GetMineSpotRange virtual
// (vtbl+0x168) -- `return fDistance <= GetMineSpotRange( nDC );`.
bool CUnitMission::CanSeeMine( float fDistance, int nDC )
{
	return fDistance <= GetMineSpotRange( nDC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMission::CanClear( int nDC, int nSkillModif )
{
	int nEngSkill = GetRPGUnit()->Skills( NDb::ST_ENGINEERING );
	float fParam;
	if ( HasPerk( N_PERK_DISARM_TRAPS, &fParam ) )
		nEngSkill = int( nEngSkill + double( fParam ) );
	int nProb = Min( pMinesConstants->nBaseDisarmProb + nEngSkill - nDC, 95 );
	return random.Get( 0, 100 ) < nProb;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Variant for creating a unit from an NRPG::Unit, for the player's characters
IUnitMission* CreateUnit( CUnit *pSrc )
{
	static int nUnitN = 0;
	CUnitMission *pRes = new CUnitMission();
	pRes->pRPGUnit = pSrc; 
	wstring szDotString;
	NStr::ToDotString( &szDotString, ++nUnitN );
	pRes->sID = L"Pers#" + szDotString;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Variant for creating a monster
// pInHandItem/pBackpack = the map builder's rolled loot (retail NRPG::CreateUnit @0x2c4f50 params
// 4/5 from SMapUnit); they thread into the CUnit ctor and replace the pers-default equipment.
IUnitMission* CreateUnit( NDb::CRPGPers *pSrc, NDb::CRPGItem *pInHandItem, NDb::CRPGChestReal *pBackpack )
{
	static int nUnitN = 0;
	CUnitMission *pRes = new CUnitMission();
	// v1.2 0x6c5131: map-created units do not collect medals (mercenaries do).
	pRes->pRPGUnit = new CUnit( pSrc, 0, false, 0, pInHandItem, pBackpack, true );
	if ( IsValid( pSrc->pDefaultWearsPanzerklein ) )
		pRes->GetRPGUnit()->pPanzerklein = pSrc->pDefaultWearsPanzerklein;
	NStr::ToDotString( &pRes->sID, ++nUnitN );
	pRes->sID = L"Enemy#" + pRes->sID;
	// retail @0x2c4f50: gate on the pers's OWN Panzerklein backlink (this pers IS a PK), not the
	// mission pPanzerklein (unset here); lift VP to nMaxVP via a modifier
	if ( IsValid( pSrc->pPanzerklein ) )
		pRes->InitAsPanzerklein( pSrc->pPanzerklein );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x02511016, CUnitMission );
BASIC_REGISTER_CLASS( IUnitMission );
BASIC_REGISTER_CLASS( IUnitMissionInfo );
