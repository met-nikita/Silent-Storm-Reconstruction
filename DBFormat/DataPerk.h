#ifndef __DATAPERK_H_
#define __DATAPERK_H_
//
#include "..\ADOImport\BasicDB.h"
//
namespace NDb
{
//
const int N_PERK_PARAMS = 3;
const int N_PERK_PARENTS = 3;
class CString;
class CUITexture;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPerk
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBPerk: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBPerk );
public:
	ZDATA
	ZPARENT( CDBRecord );
	string szUserName;
	vector<float> params;
	////
	string szID;
	CPtr<CString> pToolTip;
	CPtr<CUITexture> pIcon;
	CPtr<CUITexture> pIconDisabled;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&szUserName); f.Add(4,&params); f.Add(5,&szID); f.Add(6,&pToolTip); f.Add(7,&pIcon); f.Add(8,&pIconDisabled); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPerkTreeNode
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBPerkTreeNode: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBPerkTreeNode );
public:
	ZDATA
	ZPARENT( CDBRecord );
	int nTreeID;
	CPtr<CDBPerk> pPerk;
	vector< CPtr<CDBPerk> > parentPerks;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nTreeID); f.Add(4,&pPerk); f.Add(5,&parentPerks); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBPerk *GetDBPerk( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __DATAPERK_H_