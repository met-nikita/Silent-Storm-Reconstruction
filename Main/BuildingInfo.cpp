#include "StdAfx.h"
#include "BuildingInfo.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "Grid.h"
#include "..\Misc\2Darray.h"

namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildInfo::CBuildInfo()
{
	nMinFloor = 0;
	nMaxFloor = 0;
	nMaxY = 0;
	nMaxX = 0;
	nMaxSpotID = 1;
	nMaxLadderID = 1;
	nMaxFragmentID = 1;
	cellar.FillEvery( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildInfoLoader::Recalc()
{
	pValue = new CBuildInfo;
	try
	{
		NGScene::CResourceOpener file( "Buildings", GetKey() );
		
		pValue->operator&( *(file.operator->()) );
		if ( pValue->roomMap.empty() )
		{
			CArray2D<BYTE> array( pValue->nMaxX + 2, pValue->nMaxY + 2 ); // добавляем по краям полоску в 1 тайл
			array.FillZero();
			pValue->roomMap.resize( pValue->nMaxFloor - pValue->nMinFloor + 1, array );
		}
	}
	catch(...)
	{
		OutputDebugString( "Exception: CBuildInfoLoader::Recalc()\n" );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildFragment::operator&( CStructureSaver &f )
{ 
	f.Add( 2, &nConstructionPartID );
	f.Add( 3, &ptPos );
	f.Add( 4, &nRotationID );
	f.Add( 5, &nSubBlockID );
	f.Add( 6, &nFragmentID );
	for ( int i = 0; i < NDb::N_CONSTRUCTION_MATERIALS; ++i )
		f.Add( 10 + i, &materials[i] );
	f.Add( 30, &nObjectFlags );
	f.Add( 31, &spots );
	f.Add( 32, &nID );
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMixedMaterial* SRawMixedMaterial::CreateMixedMaterial( SRand *pRand ) const
{
	CMixedMaterial *pM = new CMixedMaterial;
	for ( int i = 0; i < layers.size(); ++i )
	{
		const SRawMaterialApply &ra = layers[i];
		if ( ra.nTMaterialID <= 0 )
			continue;
		NDb::CTMaterial *pTM = NDb::GetTMaterial( ra.nTMaterialID );
		if ( !pTM )
			continue;
		SMaterialApply a;
		a.pMaterial = pTM->GetMaterial( pRand );
		if ( !IsValid( a.pMaterial ) )
			continue;
		a.mapping   = ra.mapping;
		a.fScale    = ra.fScale;
		a.ptShift   = ra.ptShift;
		a.nRotation = ra.nRotation;
		pM->layers.push_back( a );
	}
	if ( !pM->layers.empty() )
		return pM;
	// нет действительных материалов в списке, удаляем pM
	CPtr<CMixedMaterial> p = pM;
	p = 0;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void FindFragments4Spot( vector<int> *pRes, int nSpotID, const vector<SBuildFragment> &frags )
{
	pRes->clear();
	for ( int i = 0; i < frags.size(); ++i )
	{
		if ( find( frags[i].spots.begin(), frags[i].spots.end(), nSpotID ) != frags[i].spots.end() )
			pRes->push_back( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildInfo::GetSpotFragments( int nSpotID, vector<int> *pSolids, vector<int> *pWalls ) const
{
	FindFragments4Spot( pSolids, nSpotID, solidFragments );
	FindFragments4Spot( pWalls, nSpotID, wallFragments );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuildInfo::CreateNextSpotID()
{
	int nRet = nMaxSpotID <= 0 || nMaxSpotID > 100000 ? 1 : nMaxSpotID;  // >100000 for old templates
	nMaxSpotID = nRet + 1;
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuildInfo::CreateNextFragmentID()
{
	int nRet = nMaxFragmentID <= 0 || nMaxFragmentID > 100000 ? 1 : nMaxFragmentID;  // >100000 for old templates
	nMaxFragmentID = nRet + 1;
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuildInfo::CreateNextLadderID()
{
	int nRet = nMaxLadderID <= 0 || nMaxLadderID > 100000 ? 1 : nMaxLadderID;  // >100000 for old templates
	nMaxLadderID = nRet + 1;
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
