#include "StdAfx.h"
#include "BuildingClip.h"

namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SClipInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnpackParts( vector<int> *pParts, DWORD parts, int nPartID )
{
	if ( UNBROKEN_BLOCK32 == parts )
	{
		pParts->push_back( nPartID );
		return;
	}
	//
	for ( int z = 0; z < 6; z += 2 )
	{
		int offset = (z / 2) * 9;
		if ( parts & (1 << offset) )
			pParts->push_back( nPartID | GetPieceHashID( 1, 1, z + 2 ) );
		if ( parts & (1 << (offset+1)) )
			pParts->push_back( nPartID | GetPieceHashID( 2, 1, z + 1 ) );
		if ( parts & (1 << (offset+2)) )
			pParts->push_back( nPartID | GetPieceHashID( 3, 1, z + 2 ) );
		
		if ( parts & (1 << (offset+3)) )
			pParts->push_back( nPartID | GetPieceHashID( 1, 2, z + 1 ) );
		if ( parts & (1 << (offset+4)) )
			pParts->push_back( nPartID | GetPieceHashID( 2, 2, z + 2 ) );
		if ( parts & (1 << (offset+5)) )
			pParts->push_back( nPartID | GetPieceHashID( 3, 2, z + 1 ) );

		if ( parts & (1 << (offset+6)) )
			pParts->push_back( nPartID | GetPieceHashID( 1, 3, z + 2 ) );
		if ( parts & (1 << (offset+7)) )
			pParts->push_back( nPartID | GetPieceHashID( 2, 3, z + 1 ) );
		if ( parts & (1 << (offset+8)) )
			pParts->push_back( nPartID | GetPieceHashID( 3, 3, z + 2 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SplitOptimized( vector<int> *pPartsHash )
{
	if ( pPartsHash->empty() )
		return;
	// check if already split
	if ( pPartsHash->size() > 1 )
		return;
	int nPartID = *pPartsHash->begin();
	int x, y, z;
	GetPartCoords( nPartID, &x, &y, &z );
	const int nPart = GetPartHashID( x, y, z );
	// single piece but already splitted
	if ( nPartID != nPart )
		return;
	pPartsHash->resize( 0 );
	for ( int z = 1; z < 6; ++z )
	{
		pPartsHash->push_back( nPart | GetPieceHashID( 2, 1, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 1, 2, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 3, 2, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 2, 3, z ) );
		if ( ++z > 5 )
			break;
		pPartsHash->push_back( nPart | GetPieceHashID( 1, 1, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 3, 1, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 2, 2, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 1, 3, z ) );
		pPartsHash->push_back( nPart | GetPieceHashID( 3, 3, z ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
