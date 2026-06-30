#ifndef __DATARPGCONSTANTS_H_
#define __DATARPGCONSTANTS_H_
//
#include "..\ADOImport\BasicDB.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBMinesConstants
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBMinesConstants: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBMinesConstants );
public:
	ZDATA
	ZPARENT( CDBRecord );
	int nBaseDisarmProb;
	int nSpotSkillModif;
	int nMinerEngineerSkillModif;
	int nMineSpotModif;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nBaseDisarmProb); f.Add(4,&nSpotSkillModif); f.Add(5,&nMinerEngineerSkillModif); f.Add(6,&nMineSpotModif); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBMinesConstants *GetDBMinesConstants();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __DATARPGCONSTANTS_H_