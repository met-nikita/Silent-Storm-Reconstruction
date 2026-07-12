#ifndef __DATAMISC_H_
#define __DATAMISC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Standalone DB records added in the release but absent from the dev snapshot: medals, action-point
// constants, picklocks, UI hints/cursors. Member layout, operator& tags and Import() column names are
// verbatim from the matched Game.exe + PDB. Registered in DataFormat.cpp's RegisterDatabaseClasses().
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSide;
class CString;
class CUITexture;
class CRPGItem;
class CDBPerk;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedal (table "Medals")
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMedal: public CDBRecord
{
	OBJECT_BASIC_METHODS( CMedal );
public:
	CPtr<CSide>      pSide;                   // +0x10
	CPtr<CMedal>     pPrecedingMedal;         // +0x14
	int              nStartingProbability;    // +0x18
	int              nAddToProbability;       // +0x1c
	int              nPointsToStart;          // +0x20
	int              nPointsToAddProbability; // +0x24
	CPtr<CString>    pName;                   // +0x28
	CPtr<CRPGItem>   pModel;                  // +0x2c
	CPtr<CUITexture> pImage;                  // +0x30
	bool             bIsRussianOnly;          // +0x34

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGAP (table "RPGAPs")
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGAP: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRPGAP );
public:
	int nAP;  // +0x10

	virtual void Import();
	int operator&( CStructureSaver &f );
};
// retail NDb::GetRPGAP @0x3f9b60 -- RPGAP table record by id. Ids consumed by CUnitMission::GetActionAP
// @0x2c0bd0: 1=TAKE_CORPSE 2=TRAP_OBJECT 3=DISARM_TRAP 4=DISARM_MINE 5=THROW_GRENADE 6=APPROACH_CANNON
// 7=ENTER_PK 8=LEAVE_PK 9=USE_KEY 10=SWAP 11=ITEM_TAKE 12=ITEM_SLOT 13=ITEM_TRANSFER.
CRPGAP *GetRPGAP( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGPicklock (table "RPGPicklocks")
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGPicklock: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRPGPicklock );
public:
	CPtr<CRPGItem> pItem;          // +0x10
	int            nAPToUse;       // +0x14
	int            nNeededEngSkill; // +0x18
	CPtr<CDBPerk>  pNeededPerk;    // +0x1c
	int            nAddToEngSkill; // +0x20
	int            nQuantity;      // +0x24

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIHint (table "Hints")
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIHint: public CDBRecord
{
	OBJECT_BASIC_METHODS( CUIHint );
public:
	int           nSequenceID; // +0x10
	CPtr<CString> pTitle;      // +0x14
	CPtr<CString> pString;     // +0x18

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICursor (table "UICursors")
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICursor: public CDBRecord
{
	OBJECT_BASIC_METHODS( CUICursor );
public:
	CPtr<CUITexture> pUITexture; // +0x10
	int              nCenterX;   // +0x14
	int              nCenterY;   // +0x18
	int              nHWCenterX; // +0x1c
	int              nHWCenterY; // +0x20
	string           szFileName; // +0x24

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAMISC_H_
