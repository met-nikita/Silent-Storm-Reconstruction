#include "StdAfx.h"
#include "DataAI.h"
#include "DataSound.h"

namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISound
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAISound::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &fRadius );
	f.Add( 3, &vRadius[0] );
	f.Add( 4, &vRadius[1] );
	f.Add( 5, &vRadius[2] );
	f.Add( 6, &vRadius[3] );
	f.Add( 7, &vRadius[4] );
	f.Add( 8, &pSound );
	f.Add( 9, &bTileTypeIndependent );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAISound::Import()
{
	NDatabase::ImportField( "Radius", &fRadius );
	NDatabase::ImportField( "SoundID", &pSound );
	NDatabase::ImportField( "R1", &vRadius[0] );
	NDatabase::ImportField( "R2", &vRadius[1] );
	NDatabase::ImportField( "R3", &vRadius[2] );
	NDatabase::ImportField( "R4", &vRadius[3] );
	NDatabase::ImportField( "R5", &vRadius[4] );
	int nTileTypeIndependent;
	NDatabase::ImportField( "TileTypeIndependent", &nTileTypeIndependent );
	bTileTypeIndependent = ( nTileTypeIndependent != 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAISound::GetRadiusFromAISoundType( int nAISoundType )
{
	if ( bTileTypeIndependent )
		return fRadius;
	else
		return vRadius[ nAISoundType ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitGroup::Import()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
REGISTER_SAVELOAD_CLASS( 0x01612140, CAISound )
REGISTER_SAVELOAD_CLASS( 0xA11A2140, CUnitGroup );
