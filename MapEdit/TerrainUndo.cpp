#include "StdAfx.h"
#include "TerrainUndo.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\METerrain.h"
#include "weInterface.h"
#include "..\Main\Grid.h"
#include "MESerialize.h"

externA5 CBasicShare<int, CMETerrainLoader> shareTerrains;
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainUndo::CTerrainUndo()
{
	nGrassLayer = -1;
	bEmpty = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainUndo::SetGrassLayer( int nLayerID )
{
	ASSERT( nGrassLayer == -1 );
	nGrassLayer = nLayerID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainUndo::PushTextureOp( const STextureOp &op )
{
	texture.push_back( op );
	bEmpty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainUndo::PushGrassOp( const SGrassOp &op )
{
	grass.push_back( op );
	bEmpty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainUndo::PushBlade( bool bInsert, const CVec2 &pt )
{
	if ( bInsert )
		bladesInserted.push_back( pt );
	else
		bladesDeleted.push_back( pt );
	bEmpty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainUndo::SetHeightMapOp( const CArray2D<unsigned short> &oldhm, const CArray2D<unsigned short> &newhm, const CTRect<float> &rUpdate )
{
	ASSERT( hm.empty() );
	hm.resize( 1 );
	hm[0].x = rUpdate;
	hm[0].y = rUpdate;
	hm[0].oldValue = oldhm;
	hm[0].newValue = newhm;
	bEmpty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMETerrainInfo* CTerrainUndo::GetTerrain()
{
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	if ( !IsValid( pW ) )
		return 0;
	CDGPtr< CPtrFuncBase<CMETerrainInfo> > pLoader = shareTerrains.Get( pW->GetWorldID() );
	pLoader.Refresh();
	return pLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SGrassLayer* FindGrassLayer( CMETerrainInfo *pTerr, int nLayerID )
{
	for ( int i = 0; i < pTerr->info.grass.size(); ++i )
		if ( nLayerID == pTerr->info.grass[i].nID )
			return &pTerr->info.grass[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void MinMaxRect( CTRect<float> *pRect, const T &pt )
{
	pRect->minx = Min( pRect->minx, (float)pt.x );
	pRect->miny = Min( pRect->miny, (float)pt.y );
	pRect->maxx = Max( pRect->maxx, (float)pt.x );
	pRect->maxy = Max( pRect->maxy, (float)pt.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCoord, class TValue>
bool DoUndoOperations( CTRect<float> *pRect, CArray2D<TValue> *pArray, const vector<SOperation<TCoord, TValue> > &ops )
{
	if ( ops.empty() )
		return false;
	*pRect = CTRect<float>( ops[0].x, ops[0].y, ops[0].x, ops[0].y );
	for ( int i = 0; i < ops.size(); ++i )
	{
		const SOperation<TCoord, TValue> &op = ops[i];
		(*pArray)[op.y][op.x] = op.oldValue;
		MinMaxRect( pRect, op );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCoord, class TValue>
bool DoRedoOperations( CTRect<float> *pRect, CArray2D<TValue> *pArray, const vector<SOperation<TCoord, TValue> > &ops )
{
	if ( ops.empty() )
		return false;
	*pRect = CTRect<float>( ops[0].x, ops[0].y, ops[0].x, ops[0].y );
	for ( int i = 0; i < ops.size(); ++i )
	{
		const SOperation<TCoord, TValue> &op = ops[i];
		(*pArray)[op.y][op.x] = op.newValue;
		MinMaxRect( pRect, op );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool DeleteBlades( CTRect<float> *pRect, SGrassLayer *pGrass, const vector<CVec2> &blades )
{
	bool bRet = false;
	*pRect = CTRect<float> ( 1e6, 1e6, -1e6, -1e6 );
	for ( vector<CVec2>::iterator i = pGrass->blades.begin(); i != pGrass->blades.end(); )
	{
		const CVec2 &pt = *i;
		bool b = false;
		for ( int j = 0; j < blades.size(); ++j )
			if ( fabs( pt - blades[j] ) < FP_EPSILON )
			{
				i = pGrass->blades.erase( i );
				MinMaxRect( pRect, pt );
				b = true;
				break;
			}
		if ( b )
			bRet = true;
		else
			++i;
	}
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool InsertBlades( CTRect<float> *pRect, SGrassLayer *pGrass, const vector<CVec2> &blades )
{
	pGrass->blades.insert( pGrass->blades.end(), blades.begin(), blades.end() );
	if ( blades.empty() )
		return false;
	*pRect = CTRect<float>( blades[0].x, blades[0].y, blades[0].x, blades[0].y );
	for ( int i = 1; i < blades.size(); ++i )
		MinMaxRect( pRect, blades[i] );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CTRect<float> operator*( float f, const CTRect<float> r )
{
	return CTRect<float>( f * r.minx, f * r.miny, f * r.maxx, f * r.maxy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainUndo::DoUndo( NMapEditor::CPostProcessQueue *pQueue )
{
	CMETerrainInfo *pTerr = GetTerrain();
	if ( !IsValid( pTerr ) )
		return false;
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();// раз есть pTerr, то здесь можно без проверок
	CTRect<float> r;

	if ( DoUndoOperations( &r, &pTerr->info.typeMap, texture ) )
		pW->InvalidateTerrainTextureRect( FP_GRID_STEP * r );
	SGrassLayer *pGrass = FindGrassLayer( pTerr, nGrassLayer );
	if ( pGrass )
	{
		if ( DoUndoOperations( &r, &pGrass->grass, grass ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
		if ( DeleteBlades( &r, pGrass, bladesInserted ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
		if ( InsertBlades( &r, pGrass, bladesDeleted ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
	}
	if ( !hm.empty() )
	{
		pTerr->info.heightMap = hm.front().oldValue;
		pW->InvalidateTerrainGeometryRect( hm.front().x );
	}
	SerializeTerrain( pTerr, pW->GetWorldID() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainUndo::DoRedo( NMapEditor::CPostProcessQueue *pQueue )
{
	CMETerrainInfo *pTerr = GetTerrain();
	if ( !IsValid( pTerr ) )
		return false;
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	CTRect<float> r;

	if ( DoRedoOperations( &r, &pTerr->info.typeMap, texture ) )
		pW->InvalidateTerrainTextureRect( FP_GRID_STEP * r );
	SGrassLayer *pGrass = FindGrassLayer( pTerr, nGrassLayer );
	if ( pGrass )
	{
		if ( DoRedoOperations( &r, &pGrass->grass, grass ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
		if ( DeleteBlades( &r, pGrass, bladesDeleted ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
		if ( InsertBlades( &r, pGrass, bladesInserted ) )
			pW->InvalidateTerrainGrassRect( FP_GRID_STEP * r );
	}
	if ( !hm.empty() )
	{
		pTerr->info.heightMap = hm.front().newValue;
		pW->InvalidateTerrainGeometryRect( hm.front().x );
	}
	SerializeTerrain( pTerr, pW->GetWorldID() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
