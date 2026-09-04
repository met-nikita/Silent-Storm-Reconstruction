#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GMemFormat.h"
#include "RPGGlobal.h"
#include "RPGStore.h"	// NRPG::CStore -- per-player vendor stock (retail side-seeded ctor @0x29a7c0 creates it)
#include "RPGMerc.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "scScenarioTracker.h"
#include "RPGDiplomacy.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataAck.h"
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail v1.2 @0x6998d0: use the best living merc's skill plus a difficulty-controlled
// fraction of the remaining living mercs' average skill.
int CGlobalPlayer::GetPlayerSkill( NDb::ESkillType skill, float fGroupCoeff, NRPG::CUnit **ppUnit )
{
	float fRes = 0;
	int nMax = 0, nCount = 0;
	*ppUnit = 0;
	//
	for ( vector< CObj<CUnit> >::iterator i = mercs.begin(); i != mercs.end(); ++i )
	{
		if ( !(*i)->IsDead() )
		{
			int nSkill = (*i)->Skills( skill );
			if ( nMax < nSkill )
			{
				nMax = nSkill;
				*ppUnit = (*i);
			}
			fRes += nSkill;
			++nCount;
		}
	}
	//
	return ( int )( nMax + fGroupCoeff * ( nCount > 1 ? ( fRes - nMax ) / ( nCount - 1 ) : 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalPlayer::IsUnitRescued( CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	if ( !IsValid( pUnit ) )
		return false;
	//
	unordered_map< CPtr<NRPG::CUnit>, SUnitDeployData, SPtrHash >::iterator i;
	for ( i = deployData.unitsDeployData.begin(); i != deployData.unitsDeployData.end(); ++i )
		if ( i->second.pCorpse == pUnit )
			return true;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail v1.2 @0x699f30: convert the party's medical skill to a finite amount of healing.
// The Jan03 implementation passed 1000 VP to CreateFirstAid and therefore healed every merc fully.
void CGlobalPlayer::Heal( EHeal eHeal, bool bNeedCarryOutCorpse, float fHealCoeff, float fSkillCoeff )
{
	NRPG::CUnit *pUnit;
	int nSkill = GetPlayerSkill( NDb::ST_MEDICINE, fSkillCoeff, &pUnit );
	if ( !IsValid( pUnit ) )
		return;
	//
	NRPG::SFirstAid firstAid;
	firstAid.nMaxVP = ( int )( ( int )( nSkill * fHealCoeff ) * 1.7857142686843872f );
	firstAid.fdVP = ( float )firstAid.nMaxVP;
	//
	for ( vector< CObj<CUnit> >::iterator i = mercs.begin(); i != mercs.end(); ++i )
	{
		CUnit *pUnit = (*i);
		if ( !pUnit->IsDead() && ( !pUnit->IsUnconscious() || ( !bNeedCarryOutCorpse || IsUnitRescued( pUnit ) ) ) )
		{
			if ( eHeal == HEAL_BANDAGE )
				pUnit->Heal( firstAid );
			else if ( eHeal == HEAL_HEAL )
				pUnit->RegenerateVP( firstAid );
		}
		else
			pUnit->Kill();
		//
		pUnit->SetUnconscious( false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGlobalPlayer::GetAverageLevel()
{
	int nCount = 0;
	float fLevel = 0;
	for ( vector< CObj<CUnit> >::iterator i = mercs.begin(); i != mercs.end(); ++i )
	{
		if ( !(*i)->IsDead() )
		{
			fLevel += (*i)->Skills( NDb::ST_LEVEL );
			++nCount;
		}
	}
	//
	return fLevel / nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::GetAliveUnits( vector< CPtr<NRPG::CUnit> > *pUnits )
{
	ASSERT( pUnits != 0 );
	if ( pUnits == 0 )
		return;
	//
	pUnits->clear();
	for ( vector<CObj<CUnit> >::iterator i = mercs.begin();	i != mercs.end(); ++i )
	{
		if ( !(*i)->IsDead() )
			pUnits->push_back( (*i).GetPtr() );
	}
	//
	unordered_map< CPtr<NRPG::CUnit>, SUnitDeployData, SPtrHash >::iterator i;
	for ( i = deployData.unitsDeployData.begin(); i != deployData.unitsDeployData.end(); ++i )
		if ( IsValid( i->second.pCorpse ) && !i->second.pCorpse->IsDead() && 
			find( pUnits->begin(), pUnits->end(), i->second.pCorpse.GetPtr() ) == pUnits->end() )
				pUnits->push_back( i->second.pCorpse.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::Hire( CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	bool bNotExist = find( mercs.begin(), mercs.end(), pUnit ) == mercs.end();
	ASSERT( bNotExist );
	if ( !IsValid( pUnit ) || !bNotExist )
		return;
	//
	mercs.push_back( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::Fire( CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	bool bExist = find( mercs.begin(), mercs.end(), pUnit ) != mercs.end();
	ASSERT( bExist );
	if ( !IsValid( pUnit ) || !bExist )
		return;
	//
	pUnit->SetUnconscious( false );
	if ( pUnit->Skills( NDb::ST_VP ) <= 0 )
		pUnit->Skills( NDb::ST_VP ).SetValue( 1 );
	mercs.erase( remove( mercs.begin(), mercs.end(), pUnit ), mercs.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::CaptureUnit( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pCarrier ) );
	if ( !IsValid( pUnit ) || !IsValid( pCarrier ) )
		return;
	//
	SUnitDeployData &data = deployData.unitsDeployData[ pCarrier ];
	data.pCorpse = pUnit;
	data.bCorpseAlive = true;
	data.bCorpseEnemy = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::RescueUnit( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pCarrier ) );
	if ( !IsValid( pUnit ) || !IsValid( pCarrier ) )
		return;
	//
	SUnitDeployData &data = deployData.unitsDeployData[ pCarrier ];
	data.pCorpse = pUnit;
	data.bCorpseAlive = true;
	data.bCorpseEnemy = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::TakeUnitCorpse( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pCarrier ) );
	if ( !IsValid( pUnit ) || !IsValid( pCarrier ) )
		return;
	//
	SUnitDeployData &data = deployData.unitsDeployData[ pCarrier ];
	data.pCorpse = pUnit;
	data.bCorpseAlive = false;
	data.bCorpseEnemy = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::FreeUnit( NRPG::CUnit *pCarrier )
{
	ASSERT( IsValid( pCarrier ) );
	if ( !IsValid( pCarrier ) )
		return;
	//
	deployData.unitsDeployData[ pCarrier ].pCorpse = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalPlayer::AddMerc( CUnit *pMerc )
{
	ASSERT( IsValid( pMerc ) );
	if ( !IsValid( pMerc ) )
		return;
	//
	mercs.push_back( pMerc );
	totalMercs.push_back( pMerc );
	deployData.unitsDeployData[ pMerc ] = SUnitDeployData();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalGame* CreateGlobalGame( int nScenarioID, int nDifficultyID )
{
	CGlobalGame *pGame = new CGlobalGame();
	//pGame->pDifficulty = NDb::GetDBDifficulty( nDifficultyID );
	pGame->pDifficulty = NDb::GetDBDifficulty( 2 );
	pGame->pScenarioTracker = NScenario::CreateScenarioTracker( nScenarioID );
	//pGame->pScenarioTracker = NScenario::CreateScenarioTracker( 1 );
	return pGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalPlayer* CreateGlobalPlayer()
{
	enum
	{
		PC_SOLDIER = 54,
		PC_GRENADER = 53,
		PC_SNIPER = 14,
		PC_MEDIC = 34,
		PC_SCOUT = 2,
		PC_ENGINEER = 3
	};
	//
	CGlobalPlayer *pPlayer = new CGlobalPlayer();
	// retail CreateGlobalPlayer() @0x29acf0 constructs via CGlobalPlayer(CSide* = NULL) @0x29a7c0:
	// a null side falls back to GetDBSide(1) (disasm: mov ecx,1 / call GetDBSide) and every player
	// gets its per-player store (step 6: pStore = new CStore(this)).
	pPlayer->pSide = NDb::GetDBSide( 1 );
	pPlayer->pStore = new CStore( pPlayer );
	pPlayer->AddMerc( CreateMerc( NDb::GetPers(PC_SOLDIER), 0, true ) );
	pPlayer->AddMerc( CreateMerc( NDb::GetPers(PC_GRENADER), 0, true ) );
	pPlayer->AddMerc( CreateMerc( NDb::GetPers(PC_SNIPER), 0, true ) );
	return pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalPlayer* CreateGlobalPlayer( NDb::CSide* pSide )
{
	CGlobalPlayer *pPlayer = new CGlobalPlayer();
	pPlayer->pSide = pSide;
	// retail side-seeded ctor @0x29a7c0: step 5 -- a null side falls back to GetDBSide(1) (plain
	// null check in the disasm); step 6 -- pStore = new CStore(this), the per-player store with a
	// back-pointer to the owning player (save tag 3).
	if ( pPlayer->pSide == 0 )
		pPlayer->pSide = NDb::GetDBSide( 1 );
	pPlayer->pStore = new CStore( pPlayer );

	{
		CDBTable<NDb::CRPGPers> *pPersTable = NDatabase::GetTable<NDb::CRPGPers>();
		CDBIterator<NDb::CRPGPers> iTempPers( *pPersTable );

		while( iTempPers.MoveNext() )
		{
			NDb::CRPGPers *pRPGPers = iTempPers.Get();

			// retail NRPG::AddTeamMngPerses @0x29adb0 (disasm-verified): the recruit roster takes ONLY
			// personas with CanHired set (byte @+0xc0) AND the matching side -- without the CanHired
			// gate every civilian/mob persona of the side floods the recruit menu.
			if ( !pRPGPers->bCanHired )
				continue;
			if ( pRPGPers->pSide != pSide )
				continue;

			pPlayer->totalMercs.push_back( CreateMerc( pRPGPers ) );
		}
	}

	return pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalPlayer* CreateGlobalPlayer( const vector<int> &personages )
{
	CGlobalPlayer *pPlayer = new CGlobalPlayer();
	// retail CreateGlobalPlayer(vector<int>&) @0x29af80 also constructs via CGlobalPlayer(NULL)
	// @0x29a7c0 -- default side GetDBSide(1) + the per-player store.
	pPlayer->pSide = NDb::GetDBSide( 1 );
	pPlayer->pStore = new CStore( pPlayer );
	//
	for ( int i = 0; i < personages.size(); ++i )
	{
		NDb::CRPGPers *pMerc = NDb::GetPers( personages[i] );
		if ( IsValid( pMerc ) )
			pPlayer->AddMerc( CreateMerc( pMerc, 0, true ) );
	}
	//
	return pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalGame::ChangeDifficulty( int nID )
{
	CDBPtr<NDb::CDBDifficulty> pTmp = NDb::GetDBDifficulty( nID );
	if ( IsValid( pTmp ) )
	{
		pDifficulty = pTmp;
		csSystem << CC_RED << "Game difficulty was changed" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalGame::HealOnLeaveZone()
{
	CGlobalPlayer::EHeal eHeal = pDifficulty->bHealOnLeaveZone ? CGlobalPlayer::HEAL_HEAL :
		( pDifficulty->bBandageOnLeaveZone ? CGlobalPlayer::HEAL_BANDAGE : CGlobalPlayer::HEAL_NONE );
	if ( bWasCampOrBase )
		eHeal = CGlobalPlayer::HEAL_NONE;

	for( vector< CObj<CGlobalPlayer> >::iterator i = players.begin(); i != players.end(); ++i )
		(*i)->Heal( eHeal, pDifficulty->bNeedCarryOutUnconscious,
			pDifficulty->fHealOnLeaveZoneCoeff, pDifficulty->fGroupMedicalCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalGame::HealOnRest()
{
	for( vector< CObj<CGlobalPlayer> >::iterator i = players.begin(); i != players.end(); ++i )
		(*i)->Heal( CGlobalPlayer::HEAL_HEAL, false,
			pDifficulty->fHealOnRestCoeff, pDifficulty->fGroupMedicalCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalGame::UpdateScenarioOnLeaveZone()
{
	for ( int n = 0; n < players.size(); n++ )
	{
		NRPG::CGlobalPlayer *pPlayer = players[n];
		vector< CPtr<NRPG::CUnit> > units;
		pPlayer->GetAliveUnits( &units );
		pScenarioTracker->ProcessScenario( units );
		//
		for ( vector<CObj<CUnit> >::iterator i = pPlayer->mercs.begin(); i != pPlayer->mercs.end(); ++i )
			pPlayer->deployData.unitsDeployData[ (*i).GetPtr() ].pCorpse = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x299a60: award fXP to every unit of every player (the per-merc loop the retail open-codes).
void CGlobalGame::AddXPToAllUnits( float fXP )
{
	for ( vector< CObj<CGlobalPlayer> >::iterator p = players.begin(); p != players.end(); ++p )
		for ( vector< CObj<CUnit> >::iterator u = (*p)->mercs.begin(); u != (*p)->mercs.end(); ++u )
			if ( IsValid( *u ) )
				(*u)->AddXP( fXP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CUnit *CGlobalGame::GetHero() const
{
	vector< CObj<CGlobalPlayer> >::const_iterator p;
	for( p = players.begin(); p != players.end(); ++p )
	{
		vector< CObj<CUnit> >::const_iterator u;
		for (	u = (*p)->mercs.begin(); u != (*p)->mercs.end(); ++u )
		{
			if ( (*u)->IsHero() )
				return *u;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGlobalGame::GetDialogHeroPersID() const
{
	if ( !players.empty() && IsValid( players[0]->pSide ) )
		return players[0]->pSide->pDialogHero->nPersID;
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x02511015, CGlobalGame )
REGISTER_SAVELOAD_CLASS( 0x53082180, CGlobalPlayer )
