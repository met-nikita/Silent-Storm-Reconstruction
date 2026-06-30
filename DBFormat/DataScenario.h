#ifndef __DATASCENARIO_H_
#define __DATASCENARIO_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CString;
class CDBScenarioClue;
class CUITexture;
class CScenarioTask;
class CScenarioGoal;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EScenarioClueType
{
	CT_PERSON = 0,
	CT_ITEM,
	CT_CONCLUSION,
	N_CT_COUNT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EScenarioObjectiveType
{
	OT_CAPTURE = 0,
	OT_DESTROY,
	N_OT_COUNT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// scenario-task kind; mapped from the "Tag" column by CScenarioTask::Import (string order == value)
enum ETaskTag
{
	TT_FIND_ITEM = 0,
	TT_DESTROY_ITEM_CARRIER,
	TT_TAKE_ITEM,
	TT_CARRY_OUT_ITEM,
	TT_FIND_PERSON,
	TT_STUN_PERSON,
	TT_TAKE_PERSON,
	TT_CARRY_OUT_PERSON,
	TT_COMPLETE_BY_SCRIPT,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBScenario
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenario: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenario );
	ZDATA
public:
	ZPARENT( CDBRecord );
	string szName;
	string szDescription;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&szName); f.Add(4,&szDescription); return 0; }
	//
	CDBScenario() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBScenarioZone
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenarioZone: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenarioZone );
	ZDATA
public:
	ZPARENT( CDBRecord );
	vector<int> templatesIDs;
	int nItemSlots;
	int nPersonSlots;
	string sSmallDescription;
	CPtr<CDBScenario> pScenario;
	int nCluesMaxNumber;
	bool bCanBeRevealed;
	// --- added in release (tags 10..14, appended; 2..9 unchanged) ---
	vector<string> vszParams;
	CPtr<CUITexture> pPWLImage;
	CPtr<CString> pName;
	bool bAllowPK;
	int nMaxDifficulty;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&templatesIDs); f.Add(4,&nItemSlots); f.Add(5,&nPersonSlots); f.Add(6,&sSmallDescription); f.Add(7,&pScenario); f.Add(8,&nCluesMaxNumber); f.Add(9,&bCanBeRevealed); f.Add(10,&vszParams); f.Add(11,&pPWLImage); f.Add(12,&pName); f.Add(13,&bAllowPK); f.Add(14,&nMaxDifficulty); return 0; }
	//
	CDBScenarioZone() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBScenarioState
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenarioState: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenarioState );
	ZDATA
public:
	ZPARENT( CDBRecord );
	string sDescription;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&sDescription); return 0; }
	//
	CDBScenarioState() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBScenarioObjective
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenarioObjective: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenarioObjective );
	ZDATA
public:
	ZPARENT( CDBRecord );
	EScenarioObjectiveType type;
	vector< CPtr<CDBScenarioZone> > zonesToOpen;
	vector< CPtr<CDBScenarioZone> > zonesToBlock;
	CPtr<CString> pDescription;
	CPtr<CDBScenario> pScenario;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&type); f.Add(4,&zonesToOpen); f.Add(5,&zonesToBlock); f.Add(6,&pDescription); f.Add(7,&pScenario); return 0; }
	//
	CDBScenarioObjective() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBScenarioClue
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenarioClue: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenarioClue );
	ZDATA
public:
	ZPARENT( CDBRecord );
	EScenarioClueType clueType;
	CPtr<CDBScenarioState> pState;
	vector< CPtr<CDBScenarioZone> > zonesToPlace;
	vector< CPtr<CDBScenarioObjective> > objectives;
	string sSmallDescription;
	CPtr<CDBScenario> pScenario;
	int nItemID;
	int nPersID;
	bool bPermanent;
	int nMinParentToOpen;
	CPtr<CString> pDescription;
	bool bGiveImmediately;
	// --- added in release; release also REORDERED the serialization tags (see operator&) ---
	bool bGiveByScript;
	CPtr<CScenarioGoal> pGoal;	// release tag 16; populated from the "GoalID" column
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nItemID); f.Add(4,&nPersID); f.Add(5,&nMinParentToOpen); f.Add(6,&bPermanent); f.Add(7,&bGiveByScript); f.Add(8,&bGiveImmediately); f.Add(9,&sSmallDescription); f.Add(10,&pDescription); f.Add(11,&clueType); f.Add(12,&pScenario); f.Add(13,&pState); f.Add(14,&zonesToPlace); f.Add(15,&objectives); f.Add(16,&pGoal); return 0; }
	//
	CDBScenarioClue() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBScenarioObjective2Clue: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBScenarioObjective2Clue );
	ZDATA
public:
	ZPARENT( CDBRecord );
	CPtr<CDBScenarioObjective> pObjective;
	CPtr<CDBScenarioClue> pClue;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&pObjective); f.Add(4,&pClue); return 0; }
	//
	CDBScenarioObjective2Clue() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioTask - one objective of a CScenarioGoal (release table 0x6e ScenarioTasks).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioTask: public CDBRecord
{
	OBJECT_BASIC_METHODS( CScenarioTask );
public:
	ZDATA_(CDBRecord)
	string szTag;
	ETaskTag eTag;
	CPtr<CString> pDescription;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&szTag); f.Add(3,&eTag); f.Add(4,&pDescription); return 0; }
	//
	CScenarioTask() : eTag( TT_FIND_ITEM ) {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioGoal - a named cluster of CScenarioTasks (release table 0x6f ScenarioGoals).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioGoal: public CDBRecord
{
	OBJECT_BASIC_METHODS( CScenarioGoal );
public:
	ZDATA_(CDBRecord)
	CPtr<CString> pName;
	vector< CPtr<CScenarioTask> > tasks;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pName); f.Add(3,&tasks); return 0; }
	//
	CScenarioGoal() {}
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __DATASCENARIO_H_