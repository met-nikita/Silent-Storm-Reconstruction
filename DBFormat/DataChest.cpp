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
bool IsSuitableVariant( const vector<int> &vInputParams, const vector<NDb::SVariantFlags> &flags );	// DataFormat.cpp
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
// CTRPGChest
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NDb::CTRPGChest::CreateChest @0x3fbf70 (disasm-verified):
//   1. Scan the variant list. A variant is eligible when it is valid, IsSuitableVariant(reqFlags,
//      flags) and nLevel <= nMaxLevel. Eligible variants with nLevel in [nMinLevel, nMaxLevel] join
//      a local roulette weighted by their template-roulette slot value; eligible variants BELOW
//      nMinLevel are remembered only as the single fallback with the highest such nLevel.
//   2. Pick: in-range candidates -> weighted-random pick; else the highest below-min fallback;
//      else null.
//   3. Materialize: a fresh CRPGChestReal; for each CRPGLootInstances entry emit an SLootItem whose
//      quantity is nQuantity, randomized up to nMaxQuantity when nQuantity < nMaxQuantity.
CRPGChestReal* CTRPGChest::CreateChest( SRand *pRand, const vector<int> &reqFlags, int nMinLevel, int nMaxLevel )
{
	const int n = variants.size();
	if ( n <= 0 )
		return 0;

	vector<int> candidates;
	CRoulette roll;
	int nBestBelow = -1;
	int nBestBelowIdx = -1;
	for ( int i = 0; i < n; ++i )
	{
		CRPGChest *pVariant = variants[i];
		if ( !IsValid( pVariant ) )
			continue;
		if ( !IsSuitableVariant( reqFlags, pVariant->flags ) )
			continue;
		if ( pVariant->nLevel > nMaxLevel )
			continue;
		if ( pVariant->nLevel < nMinLevel )
		{
			if ( nBestBelow < pVariant->nLevel )
			{
				nBestBelow = pVariant->nLevel;
				nBestBelowIdx = i;
			}
		}
		else
		{
			candidates.push_back( i );
			roll.AddSector( roulette.GetSectorValue( i ) );
		}
	}

	CRPGChest *pChosen = 0;
	if ( candidates.empty() )
	{
		if ( nBestBelowIdx == -1 )
			return 0;
		pChosen = variants[nBestBelowIdx];
	}
	else
		pChosen = variants[candidates[roll.GetRandomSector( pRand )]];

	if ( !IsValid( pChosen ) )
		return 0;

	CRPGChestReal *pReal = new CRPGChestReal;
	for ( int k = 0; k < pChosen->instances.size(); ++k )
	{
		CRPGLootInstances *pLoot = pChosen->instances[k];
		SLootItem item;
		item.pItem = pLoot->pItem;
		item.nQuantity = pLoot->nQuantity;
		if ( pLoot->nQuantity < pLoot->nMaxQuantity )
			item.nQuantity += pRand->Get( (pLoot->nMaxQuantity - pLoot->nQuantity) + 1 );
		item.pDifficulty = pLoot->pDifficulty;
		pReal->items.push_back( item );
	}
	return pReal;
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
