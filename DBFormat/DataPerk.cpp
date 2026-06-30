#include "StdAfx.h"
//
#include "DataFormat.h"
#include "DataPerk.h"
#include "DataInterface.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPerk
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBPerk::Import()
{
	NDatabase::ImportField( "UserName", &szUserName );
	params.resize( N_PERK_PARAMS );
	NDatabase::ImportField( "Param1", &params[0] );
	NDatabase::ImportField( "Param2", &params[1] );
	NDatabase::ImportField( "Param3", &params[2] );
	////
	NDatabase::ImportField( "IDText", &szID );
	NDatabase::ImportField( "ToolTip", &pToolTip );
	NDatabase::ImportField( "Icon", &pIcon );
	NDatabase::ImportField( "IconDisabled", &pIconDisabled );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPerkTreeNode
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBPerkTreeNode::Import()
{
	NDatabase::ImportField( "PerkTreeID", &nTreeID );
	NDatabase::ImportField( "PerkID", &pPerk );
	parentPerks.resize( N_PERK_PARENTS );
	NDatabase::ImportField( "ParentPerkID1", &parentPerks[0] );
	NDatabase::ImportField( "ParentPerkID2", &parentPerks[1] );
	NDatabase::ImportField( "ParentPerkID3", &parentPerks[2] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x52612160, CDBPerk );
REGISTER_SAVELOAD_CLASS( 0x52612161, CDBPerkTreeNode );