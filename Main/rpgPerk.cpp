#include "stdafx.h"
//
#include "..\DBFormat\DataPerk.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\RandomGen.h"		// SRand (TakeRandomPerks)
#include "rpgPerk.h"
//
#include <fstream>
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerk
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerk::AddParent( CPerk *pParent )
{
	if ( !IsValid( pParent ) )
		return;
	//
	ASSERT( !parents.IsContain( pParent ) );
	parents.Add( pParent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CPerk::CanTake() const
{
	if ( bTaken )
		return false;
	//
	int nSize = parents.GetSize();
	for ( int i = 0; i < nSize; ++i )
		if ( !parents[i]->IsTaken() )
			return false;
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerksTree
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerk *CreatePerk( NDb::CDBPerk *pDBPerk );
//
CPerk* CPerksTree::GetOrCreatePerk( NDb::CDBPerk *pDBPerk )
{
	ASSERT( IsValid( pDBPerk ) );
	if ( !IsValid( pDBPerk ) )
		return 0;
	//
	CObj<CPerk> &pPerk = perks[ pDBPerk->GetRecordID() ];
	if ( !IsValid( pPerk ) )
		pPerk = CreatePerk( pDBPerk );
	return pPerk;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::LoadTree( int nTreeID )
{
	CDBTable<NDb::CDBPerkTreeNode> *pTable = NDatabase::GetTable<NDb::CDBPerkTreeNode>();
	CDBIterator<NDb::CDBPerkTreeNode> i( *pTable );
	while ( pTable && i.MoveNext() )
	{
		CDBPtr<NDb::CDBPerkTreeNode> pDBNode = i.Get();
		if ( IsValid( pDBNode )&& pDBNode->nTreeID == nTreeID )
		{
			CPtr<CPerk> pPerk = GetOrCreatePerk( pDBNode->pPerk );
			ASSERT( IsValid( pPerk ) );
			if ( !IsValid( pPerk ) )
				continue;
			//
			const vector< CPtr<NDb::CDBPerk> > &parents = pDBNode->parentPerks;
			for ( vector< CPtr<NDb::CDBPerk> >::const_iterator p = parents.begin(); p != parents.end(); ++p )
			{
				if ( IsValid( *p ) )
					pPerk->AddParent( GetOrCreatePerk( *p ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::GetAllPerks( vector< CPtr<CPerk> > *pPerks ) const
{
	pPerks->clear();
	for (unordered_map< int, CObj<CPerk> >::const_iterator i = perks.begin(); i != perks.end(); ++i )
	{
		if ( !IsValid( i->second ) )
		{
			DebugTrace( "ERROR: Invalid perk tree!\n" );
			continue;
		}

		pPerks->push_back( i->second.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::GetTakenPerks( vector< CPtr<CPerk> > *pPerks ) const
{
	pPerks->clear();
	for (unordered_map< int, CObj<CPerk> >::const_iterator i = perks.begin(); i != perks.end(); ++i )
	{
		if ( !IsValid( i->second ) )
		{
			DebugTrace( "ERROR: Invalid perk tree!\n" );
			continue;
		}

		if ( i->second->IsTaken() )
			pPerks->push_back( i->second.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::GetAvailablePerks( vector< CPtr<CPerk> > *pPerks ) const
{
	pPerks->clear();

	if ( nPerkPoints <= 0 )
		return;

	for (unordered_map< int, CObj<CPerk> >::const_iterator i = perks.begin(); i != perks.end(); ++i )
	{
		if ( !IsValid( i->second ) )
		{
			DebugTrace( "ERROR: Invalid perk tree!\n" );
			continue;
		}
		//
		if ( nPerkPoints > 0 && i->second->CanTake() )
			pPerks->push_back( i->second.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerksTree::CPerksTree( int nTreeID ): nPerkPoints( 0 )
{
	if ( nTreeID >= 0 )
		LoadTree( nTreeID );
	//DEBUG{
	//if ( !perks.empty() )
	//	Draw();
	//DEBUG}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::Draw( string szFileName )
{
	fstream file;
	string str = szFileName + ".dot";
	file.open( str.c_str(), ios_base::out | ios_base::trunc );
	file << "digraph g\n{\n  concentrate=true;\n  nodesep=.3;\n  ranksep=.3;\n";
	// 
	for (unordered_map< int, CObj<CPerk> >::const_iterator i = perks.begin(); i != perks.end(); ++i )
	{
		char szID[128];
		sprintf( szID, "ID_%d", i->first );
		file << "  " << szID << "[shape = box,";
		if ( i->second->IsTaken() )
			file << " style=filled,";
		if ( i->second->CanTake() )
			file << " fontcolor=darkgreen,";
		file << " label = \"" << i->second->GetDBPerk()->szUserName << "\"];" << endl;
	}
	//
	for (unordered_map< int, CObj<CPerk> >::const_iterator i = perks.begin(); i != perks.end(); ++i )
	{
		char szID1[128], szID2[128];
		sprintf( szID1, "ID_%d", i->first );
		int nParentSize = i->second->GetParents().GetSize();
		for ( int j = 0; j < nParentSize; ++j )
		{
			sprintf( szID2, "ID_%d", i->second->GetParents()[j]->GetDBPerk()->GetRecordID() );
			file << "  " << szID2 << " -> " << szID1 << ";" << endl;
		}
	}
	//
	file << "}";
	file.close();
	//
	str = "scengraph\\dot.exe -Tjpg " + szFileName + ".dot -o " + szFileName + ".jpg";
	system( str.c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPerksTree::HasPerk( int nPerkID, float *pParam1, float *pParam2, float *pParam3 )
{
	CObj<CPerk> &pPerk = perks[ nPerkID ];
	if ( IsValid( pPerk ) && pPerk->IsTaken() )
	{
		vector<float> &params = pPerk->GetDBPerk()->params;
		if ( pParam1 != 0 )
			*pParam1 = params[0];
		if ( pParam2 != 0 )
			*pParam2 = params[1];
		if ( pParam3 != 0 )
			*pParam3 = params[2];
		return true;
	}
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::TakePerk( int nPerkID )
{
	CObj<CPerk> &pPerk = perks[ nPerkID ];
	if ( !IsValid( pPerk ) )
		return;
	//
	csSystem << CC_GREY << "Perk '" << CC_RED << 
		pPerk->GetDBPerk()->szUserName << CC_GREY << "' was taken" << endl;
	pPerk->Take();
	nPerkPoints = max( 0 , nPerkPoints - 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2aeb60: spend every perk point on randomly-chosen available perks. Each pass collects the
// currently-takeable perks, picks one at random, and takes it (TakePerk decrements nPerkPoints + flips the
// perk to taken so it drops out of the next pass) -- so the loop terminates when the points run out or no
// perk is takeable. TakePerk is keyed by the DB record id, which is exactly how the perks map is keyed
// (GetOrCreatePerk: perks[ pDBPerk->GetRecordID() ]).
void CPerksTree::TakeRandomPerks()
{
	while ( nPerkPoints > 0 )
	{
		vector< CPtr<CPerk> > available;
		GetAvailablePerks( &available );
		if ( available.empty() )
			break;
		//
		SRand rnd;
		int nIndex = rnd.Get( available.size() );
		TakePerk( available[ nIndex ]->GetDBPerk()->GetRecordID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPerksTree::AddPerkPoints( int nPoints )
{
	nPerkPoints = max( 0, nPerkPoints + nPoints );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerk *CreatePerk( NDb::CDBPerk *pDBPerk ) 
{ 
	return new CPerk( pDBPerk ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerksTree* CreatePerksTree( int nID )
{
	return new CPerksTree( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0x52612060, CPerk );
REGISTER_SAVELOAD_CLASS( 0x52612151, CPerksTree );