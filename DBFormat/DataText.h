#ifndef __DATATEXT_H_
#define __DATATEXT_H_
//
#include "..\ADOImport\BasicDB.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTranslatedString
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CTranslatedString +0x1c (PDB enum NDb::ETranslationStatus): localization bookkeeping state
// of the string row. Save tag 3 (operator& @0x400960, 4-byte DataChunk); Import @0x3f7da0 reads the
// raw int column "TranslationStatus".
enum ETranslationStatus
{
	TS_TRANSLATED = 0,
	TS_OUTDATED = 1,
	TS_APPROVED = 2,
	TS_UNTRANSLATED = 3,
	TS_WARNING = 4
};
class CTranslatedString: public CDBRecord
{
	OBJECT_BASIC_METHODS( CTranslatedString );
public:
	ZDATA_(CDBRecord)
	wstring szStr;
	ETranslationStatus eStatus = TS_TRANSLATED;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&szStr); f.Add(3,&eStatus); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTranslatedString* GetTranslatedString( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __DATATEXT_H_