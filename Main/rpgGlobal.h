#ifndef __RPGGLOBAL_H_
#define __RPGGLOBAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ChapterInfo.h"
#include "../DBFormat/DataDifficulty.h"
#include "../DBFormat/DataRPG.h"
#include "../DBFormat/DataMisc.h"   // complete NDb::CUIHint for CGlobalGame::hintsSet (release save-format)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScenario
{
	class CScenarioTracker;
	class CScenarioZone;
}
//
namespace NRPG
{
//
enum EChapterMapMode
{
	GGCM_NULL,
	GGCM_FIRSTTIME,
	GGCM_CONTINUE
};
//
class CUnit;
class IInventoryItem;
class CGlobalDiplomacy;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStoreItem
{
	enum EType
	{
		NONE,
		CONST_QUANTITY,
		REGEN_QUANTITY
	};

	ZDATA
	int nRating;
	EType eType;
	float fQuantity;
	CDBPtr<NDb::CRPGItem> pRPGItem;
	CDBPtr<NDb::CRPGStoreItem> pStoreItem;
	list<CObj<NRPG::IInventoryItem> > itemsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nRating); f.Add(3,&eType); f.Add(4,&fQuantity); f.Add(5,&pRPGItem); f.Add(6,&pStoreItem); f.Add(7,&itemsList); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitDeployData
{
	ZDATA
	int nPassageObjectID;
	CObj<NRPG::CUnit> pCorpse; // CObj for correctly transferring AI units between templates
	bool bCorpseAlive;
	bool bCorpseEnemy;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPassageObjectID); f.Add(3,&pCorpse); f.Add(4,&bCorpseAlive); f.Add(5,&bCorpseEnemy); return 0; }
	//
	SUnitDeployData(): nPassageObjectID( 0 ), pCorpse( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDeployData
{
	ZDATA
	bool bPassage;
	int nPassageZoneID;
	unordered_map< CPtr<NRPG::CUnit>, SUnitDeployData, SPtrHash > unitsDeployData;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bPassage); f.Add(3,&nPassageZoneID); f.Add(4,&unitsDeployData); return 0; }
	//
	SDeployData(): bPassage( false ), nPassageZoneID( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalPlayer: public CObjectBase
{
	OBJECT_BASIC_METHODS( CGlobalPlayer )
	ZDATA
public:
	CDBPtr<NDb::CSide> pSide;
	vector< CObj<CUnit> > mercs;
	vector< CObj<CUnit> > totalMercs;
	SDeployData deployData;
	list<SStoreItem> storeItemsList;
	int nMoney = 0;		// the player's cash (release: read by the Player*Money script bindings)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSide); f.Add(3,&mercs); f.Add(4,&totalMercs); f.Add(5,&deployData); f.Add(6,&storeItemsList); f.Add(7,&nMoney); return 0; }
	//
private:
	int GetPlayerSkill( NDb::ESkillType skill, NRPG::CUnit **ppUnit );
	bool IsUnitRescued( CUnit *pUnit );
public:
	CGlobalPlayer() {}
	//
	// release Player*Money script bindings: GiveMoney adds (unclamped), TakeMoney subtracts (clamped >= 0).
	int GetMoney() const { return nMoney; }
	void GiveMoney( int n ) { nMoney += n; }
	void TakeMoney( int n ) { nMoney -= n; if ( nMoney < 0 ) nMoney = 0; }
	//
	void AddMerc( CUnit *pMerc );
	//
	void CaptureUnit( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit );
	void RescueUnit( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit );
	void TakeUnitCorpse( NRPG::CUnit *pCarrier, NRPG::CUnit *pUnit );
	void FreeUnit( NRPG::CUnit *pCarrier );
	//
	void GetAliveUnits( vector< CPtr<NRPG::CUnit> > *pUnits );
	float GetAverageLevel();
	void Heal( bool bBandage, bool bNeedCarryOutCorpse, float fCoeff );
	void Hire( CUnit *pUnit );
	void Fire( CUnit *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalGame
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalGame: public CObjectBase
{
	OBJECT_BASIC_METHODS(CGlobalGame);
public:
	ZDATA
	//// profile
	wstring wsProfile;
	//// Active GlobalMap
	bool bGlobalMapSet;
	int nGlobalMapID;
	//// Active ChapterMap
	bool bChapterMapSet;
	int nChapterMapID;
	CVec2 vChapterPos;
	//// Active Zone
	int nCurrentTemplateID;
	CPtr<NScenario::CScenarioZone> pCurrentZone;
	//// Scenario
	CObj<NScenario::CScenarioTracker> pScenarioTracker;
	//// Players
	vector< CObj<CGlobalPlayer> > players;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;
	int nCurrentChapterDifficulty;
	// release save-format tags 14-22: campaign global vars / hint sequencing / hot-seat state.
	// Dead in this predecessor (no hint-sequence or hot-seat machinery); save-format members only.
	unordered_map< string, string > globalVars;
	int nHintSequenceID = 0;
	vector< CDBPtr<NDb::CUIHint> > hintsSet;
	bool bWasCampOrBase = false;
	float fXPForAliveEnemies = 0.0f;
	int nHintSequenceIDOnEnterZone = -10;
	bool bNoMoreHints = false;
	int nHotSeatTechLevel = 0;
	int nSloMoTimes = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsProfile); f.Add(3,&bGlobalMapSet); f.Add(4,&nGlobalMapID); f.Add(5,&bChapterMapSet); f.Add(6,&nChapterMapID); f.Add(7,&vChapterPos); f.Add(8,&nCurrentTemplateID); f.Add(9,&pCurrentZone); f.Add(10,&pScenarioTracker); f.Add(11,&players); f.Add(12,&pDifficulty); f.Add(13,&nCurrentChapterDifficulty); f.Add(14,&globalVars); f.Add(15,&nHintSequenceID); f.Add(16,&hintsSet); f.Add(17,&bWasCampOrBase); f.Add(18,&fXPForAliveEnemies); f.Add(19,&nHintSequenceIDOnEnterZone); f.Add(20,&bNoMoreHints); f.Add(21,&nHotSeatTechLevel); f.Add(22,&nSloMoTimes); return 0; }
	//
	CGlobalGame(): bGlobalMapSet( false ), bChapterMapSet( false ), 
		nCurrentTemplateID( -1 ), nCurrentChapterDifficulty( 0 ) {}
	//
	void ChangeDifficulty( int nID );
	void HealOnLeaveZone();
	void HealOnRest();
	void UpdateScenarioOnLeaveZone();
	// release @0x299a60 (RPGGlobal.obj): award fXP to every unit of every player. Used by the ShowHint screen
	// (NGame::CICShowHint::Exec) -- a shown hint grants its CUIHint+0x10 reward to the whole party.
	void AddXPToAllUnits( float fXP );
	NRPG::CUnit *GetHero() const;
	int GetDialogHeroPersID() const;
	// release campaign string-var store (luaSet/GetGlobalGameVar @0x2e5570 / @0x2e53f0):
	void SetGlobalVar( const string &szName, const string &szValue ) { globalVars[ szName ] = szValue; }
	string GetGlobalVar( const string &szName, const string &szDefault = "" )
	{
		unordered_map< string, string >::iterator it = globalVars.find( szName );
		return ( it != globalVars.end() ) ? it->second : szDefault;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalGame* CreateGlobalGame( int nScenarioID = -1, int nDifficultyID = 1 );
NRPG::CGlobalPlayer* CreateGlobalPlayer();
NRPG::CGlobalPlayer* CreateGlobalPlayer( NDb::CSide* pSide );
NRPG::CGlobalPlayer* CreateGlobalPlayer( const vector<int> &personages );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif