#include "StdAfx.h"
#include "DataFormat.h"
#include "DataInterface.h"
#include "DataRPG.h"
#include "DataPerk.h"
#include "DataMisc.h"
//
// Reconstructed standalone records (medals / AP / picklock / UI hint+cursor). operator& tags and
// Import column names verbatim from the release decompile (Game.exe + matched PDB).
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedal
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedal::Import()
{
	NDatabase::ImportField( "SideID", &pSide );
	NDatabase::ImportField( "PrecedingMedalID", &pPrecedingMedal );
	NDatabase::ImportField( "StartingProbabilityPercent", &nStartingProbability );
	NDatabase::ImportField( "AddToProbability", &nAddToProbability );
	NDatabase::ImportField( "PointsNeededToStart", &nPointsToStart );
	NDatabase::ImportField( "PointsNeededToAdd", &nPointsToAddProbability );
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "ImageID", &pImage );
	NDatabase::ImportField( "NameID", &pName );
	NDatabase::ImportField( "IsRussianOnly", &bIsRussianOnly );
	// retail CMedal::Import @0x42a110 tail: a live side collects this medal on its medals list
	// (retail PushItem = dedup push -- skip if already present).
	if ( IsValid( pSide ) )
	{
		bool bPresent = false;
		for ( vector< CPtr<CMedal> >::iterator it = pSide->medals.begin(); it != pSide->medals.end(); ++it )
			if ( *it == this )
			{
				bPresent = true;
				break;
			}
		if ( !bPresent )
			pSide->medals.push_back( CPtr<CMedal>( this ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMedal::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pSide );
	f.Add( 3, &pPrecedingMedal );
	f.Add( 4, &nStartingProbability );
	f.Add( 5, &nAddToProbability );
	f.Add( 6, &nPointsToStart );
	f.Add( 7, &nPointsToAddProbability );
	f.Add( 8, &pName );
	f.Add( 9, &pModel );
	f.Add( 10, &pImage );
	f.Add( 11, &bIsRussianOnly );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGAP
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGAP::Import()
{
	NDatabase::ImportField( "AP", &nAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGAP::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nAP );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGPicklock
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGPicklock::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	NDatabase::ImportField( "APToUse", &nAPToUse );
	NDatabase::ImportField( "NeededEngSkill", &nNeededEngSkill );
	NDatabase::ImportField( "NeededPerkID", &pNeededPerk );
	NDatabase::ImportField( "AddToEngSkill", &nAddToEngSkill );
	NDatabase::ImportField( "Quantity", &nQuantity );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGPicklock::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pItem );
	f.Add( 3, &nAPToUse );
	f.Add( 4, &nNeededEngSkill );
	f.Add( 5, &pNeededPerk );
	f.Add( 6, &nAddToEngSkill );
	f.Add( 7, &nQuantity );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIHint
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIHint::Import()
{
	NDatabase::ImportField( "TitleID", &pTitle );
	NDatabase::ImportField( "StringID", &pString );
	NDatabase::ImportField( "Sequence", &nSequenceID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIHint::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nSequenceID );
	f.Add( 3, &pTitle );
	f.Add( 4, &pString );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICursor (tag 3 is a removed-field gap in the release operator&)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICursor::Import()
{
	NDatabase::ImportField( "UITexture", &pUITexture );
	NDatabase::ImportField( "CenterX", &nCenterX );
	NDatabase::ImportField( "CenterY", &nCenterY );
	NDatabase::ImportField( "HWCenterX", &nHWCenterX );
	NDatabase::ImportField( "HWCenterY", &nHWCenterY );
	NDatabase::ImportField( "FileName", &szFileName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUICursor::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pUITexture );
	f.Add( 4, &nCenterX );
	f.Add( 5, &nCenterY );
	f.Add( 6, &nHWCenterX );
	f.Add( 7, &nHWCenterY );
	f.Add( 8, &szFileName );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
// retail saveload id (from initCRPGPicklock0x70122181); CRPGPicklock is an RPG item record like
// CRPGTool/CRPGKey, which carry their own saveload registration for the object-graph load path.
REGISTER_SAVELOAD_CLASS( 0x70122181, CRPGPicklock )
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
REGISTER_SAVELOAD_CLASS( 0xB3421140, CMedal )
REGISTER_SAVELOAD_CLASS( 0x01293130, CRPGAP )
REGISTER_SAVELOAD_CLASS( 0x00143110, CUICursor )
REGISTER_SAVELOAD_CLASS( 0xB3212190, CUIHint )
