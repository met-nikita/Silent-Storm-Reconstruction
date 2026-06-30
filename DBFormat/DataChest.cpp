#include "StdAfx.h"
#include "DataFormat.h"
#include "DataRPG.h"
#include "DataDifficulty.h"
#include "DataChest.h"
//
// Reconstructed RPG chest / loot cluster. Import column names + operator& tags verbatim from the
// release decompile (Game.exe + matched PDB). CRPGChest mirrors the CRndModel "variant pushes itself
// into its template pool" idiom; CRPGLootInstances wires into its owning chest via the ChestID
// relation (same pattern as the transformable-head THMID textures).
//
namespace NDb
{
externA5 void UnpackVariantFlags( const string &str, vector<SVariantFlags> *pFlags );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGLootInstances
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGLootInstances::Import()
{
	NDatabase::ImportField( "Quantity", &nQuantity );
	NDatabase::ImportField( "MaxQuantity", &nMaxQuantity );
	NDatabase::ImportField( "RPGItemID", &pItem );
	NDatabase::ImportField( "DifficultyID", &pDifficulty );
	CPtr<CRPGChest> pChest;
	NDatabase::ImportField( "ChestID", &pChest );
	if ( IsValid( pChest ) )
		pChest->instances.push_back( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGChest
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGChest::Import()
{
	NDatabase::ImportField( "Level", &nLevel );
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight = 0;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT( 0 );	// a chest variant with no parent template (release logs the record id here)
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGChestLayout
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGChestLayout::Import()
{
	NDatabase::ImportField( "VerticalLayout", &bVertical );
	NDatabase::ImportField( "SafeLayout", &bSafeLikeLayout );
	shelves.clear();
	for ( int i = 1; i < 6; ++i )
	{
		char szCol[64];
		sprintf( szCol, "ShelfHeight%d", i );
		float fHeight = 0;
		NDatabase::ImportField( szCol, &fHeight );
		if ( fabs( fHeight ) > 1e-5f )
			shelves.push_back( fHeight );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
