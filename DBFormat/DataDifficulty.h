#ifndef __DATADIFFICULTY_H_
#define __DATADIFFICULTY_H_
//
#include "..\ADOImport\BasicDB.h"
//
// Release version of CDBDifficulty, reconstructed from Game.exe (type, operator& tags @0x8005b0,
// Import columns @0x82c7b0). Serialization tags 2..15 are identical to the dev snapshot; release
// appended fClueDeathCoeff + 14 more fields as tags 16..30 (backward-compatible).
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECanSave
{
	E_CS_CHAPTER = 0,
	E_CS_REALTIME,
	E_CS_ALWAYS,
	N_CAN_SAVE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDifficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBDifficulty: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBDifficulty );
public:
	ZDATA
	ZPARENT( CDBRecord );
	float fAPCoeff;
	float fVPCoeff;
	int nAIUnitsLevel;
	float fDeathCoeff;
	bool bNeedCarryOutUnconscious;
	ECanSave canSave;
	string szUserName;
	bool bHealOnLeaveZone;
	float fHealOnLeaveZoneCoeff;
	bool bBandageOnLeaveZone;
	float fBandageOnLeaveZoneCoeff;
	float fHealOnRestCoeff;
	int nREDifficulty;
	// --- added in release ---
	float fClueDeathCoeff;
	bool bShowAllClueIcons;
	bool bShowAllScenarioGoals;
	bool bUseDefaultStringForUnknownGoals;
	float fGroupMedicalCoeff;
	int nHideProbability;
	int nAssassinProbability;
	float fEnemyDamageMult;
	float fOurDamageMult;
	bool bAlwaysCritical;
	bool bHeadshotShouldKill;
	bool bAICheckCorpses;
	float fBackstabMeleeMultiplier;
	float fBackstabMinDamageMult;
	float fBackstabMaxDamageMult;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&fAPCoeff); f.Add(4,&fVPCoeff); f.Add(5,&nAIUnitsLevel); f.Add(6,&fDeathCoeff); f.Add(7,&bNeedCarryOutUnconscious); f.Add(8,&canSave); f.Add(9,&szUserName); f.Add(10,&bHealOnLeaveZone); f.Add(11,&fHealOnLeaveZoneCoeff); f.Add(12,&bBandageOnLeaveZone); f.Add(13,&fBandageOnLeaveZoneCoeff); f.Add(14,&fHealOnRestCoeff); f.Add(15,&nREDifficulty); f.Add(16,&fClueDeathCoeff); f.Add(17,&bShowAllClueIcons); f.Add(18,&bShowAllScenarioGoals); f.Add(19,&bUseDefaultStringForUnknownGoals); f.Add(20,&fGroupMedicalCoeff); f.Add(21,&nHideProbability); f.Add(22,&nAssassinProbability); f.Add(23,&fEnemyDamageMult); f.Add(24,&fOurDamageMult); f.Add(25,&bAlwaysCritical); f.Add(26,&bHeadshotShouldKill); f.Add(27,&bAICheckCorpses); f.Add(28,&fBackstabMeleeMultiplier); f.Add(29,&fBackstabMinDamageMult); f.Add(30,&fBackstabMaxDamageMult); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBDifficulty *GetDBDifficulty( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __DATADIFFICULTY_H_
