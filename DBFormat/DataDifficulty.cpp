#include "StdAfx.h"
#include "..\Misc\StrProc.h"
#include "DataDifficulty.h"
//
// Release version: Import() column order/names taken verbatim from Game.exe (CDBDifficulty::Import
// @0x82c7b0). Identical to the dev snapshot through "REDifficulty"; release inserts "ClueDeathCoeff"
// after "DeathCoeff" and appends the difficulty/combat tuning columns.
//
namespace NDb
{
//
static const char* szCanSaveValues[ N_CAN_SAVE ] = { "chapter", "realtime", "always" };
//
template< class T >
T GetEnumByString( const string &szName, const char** szNames, int nMax )
{
	string szGoodName( szName );
	NStr::ToLower( szGoodName );
	NStr::TrimBoth( szGoodName );
	//
	for ( int i = 0; i < nMax; ++i )
		if ( szGoodName == szNames[i] )
			return ( T )i;
	return ( T )nMax;
}
//
ECanSave GetCanSaveByString( const string &szName )
{
	return GetEnumByString<ECanSave>( szName, szCanSaveValues, N_CAN_SAVE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDifficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBDifficulty::Import()
{
	string szName;
	NDatabase::ImportField( "CanSave", &szName );
	canSave = GetCanSaveByString( szName );
	NDatabase::ImportField( "AIAPCoeff", &fAPCoeff );
	NDatabase::ImportField( "AIVPCoeff", &fVPCoeff );
	NDatabase::ImportField( "DeathCoeff", &fDeathCoeff );
	NDatabase::ImportField( "ClueDeathCoeff", &fClueDeathCoeff );
	NDatabase::ImportField( "AIUnitsLevel", &nAIUnitsLevel );
	NDatabase::ImportField( "NeedCarryOutUnconscious", &bNeedCarryOutUnconscious );
	NDatabase::ImportField( "UserName", &szUserName );
	NDatabase::ImportField( "HealOnRestCoeff", &fHealOnRestCoeff );
	NDatabase::ImportField( "HealOnLeaveZone", &bHealOnLeaveZone );
	NDatabase::ImportField( "HealOnLeaveZoneCoeff", &fHealOnLeaveZoneCoeff );
	NDatabase::ImportField( "BandageOnLeaveZone", &bBandageOnLeaveZone );
	NDatabase::ImportField( "BandageOnLeaveZoneCoeff", &fBandageOnLeaveZoneCoeff );
	NDatabase::ImportField( "REDifficulty", &nREDifficulty );
	NDatabase::ImportField( "ShowAllClueIcons", &bShowAllClueIcons );
	NDatabase::ImportField( "ShowAllScenarioGoals", &bShowAllScenarioGoals );
	NDatabase::ImportField( "UseDefaultStringForUnknownGoals", &bUseDefaultStringForUnknownGoals );
	NDatabase::ImportField( "GroupMedicalCoeff", &fGroupMedicalCoeff );
	NDatabase::ImportField( "HideProbability", &nHideProbability );
	NDatabase::ImportField( "AssassinProbability", &nAssassinProbability );
	NDatabase::ImportField( "EnemyDamageMult", &fEnemyDamageMult );
	NDatabase::ImportField( "OurDamageMult", &fOurDamageMult );
	NDatabase::ImportField( "AlwaysCritical", &bAlwaysCritical );
	NDatabase::ImportField( "HeadshotShouldKill", &bHeadshotShouldKill );
	NDatabase::ImportField( "AICheckCorpses", &bAICheckCorpses );
	NDatabase::ImportField( "BackstabMeleeMultiplier", &fBackstabMeleeMultiplier );
	NDatabase::ImportField( "BackstabMinDamageMult", &fBackstabMinDamageMult );
	NDatabase::ImportField( "BackstabMaxDamageMult", &fBackstabMaxDamageMult );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x51812120, CDBDifficulty );
