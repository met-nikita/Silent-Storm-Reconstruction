#ifndef __DATAPHYS_H_
#define __DATAPHYS_H_
//
#include "..\ADOImport\BasicDB.h"
//
// Reconstructed from release Game.exe (dataphys.obj / DataFormat.obj). Member layout, serialization
// tags (operator&), Import() columns and the saveload id (0x72532130) are taken verbatim from the
// matched PDB + decompilation, so this round-trips game.db identically.
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPhysParams
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBPhysParams: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBPhysParams );
public:
	ZDATA
	ZPARENT( CDBRecord );
	float fFriction;        // +0x10
	float fResistance;      // +0x14
	float DELTA_T;          // +0x18
	DWORD DELTA_T_DWORD;    // +0x1c
	string szName;          // +0x20
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&fFriction); f.Add(4,&fResistance); f.Add(5,&DELTA_T); f.Add(6,&DELTA_T_DWORD); f.Add(7,&szName); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBPhysParams *GetDBPhysParams( const char *pszName );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __DATAPHYS_H_
