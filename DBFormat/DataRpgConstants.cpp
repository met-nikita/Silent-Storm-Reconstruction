#include "StdAfx.h"
//
#include "DataFormat.h"
#include "DataRpgConstants.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBMinesConstants
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBMinesConstants::Import()
{
	NDatabase::ImportField( "BaseDisarmProb", &nBaseDisarmProb );
	NDatabase::ImportField( "SpotSkillModif", &nSpotSkillModif );
	NDatabase::ImportField( "MinerEngineerSkillModif", &nMinerEngineerSkillModif );
	NDatabase::ImportField( "MineSpotModif", &nMineSpotModif );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x50513160, CDBMinesConstants );