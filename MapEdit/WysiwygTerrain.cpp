#include "StdAfx.h"
#include "IWysiwyg.h"
#include "WysiwygTerrain.h"
#include "weInterface.h"
#include "MEUserSettings.h"
#include "MESerialize.h"
#include "..\Main\METerrain.h"
#include "..\Main\MELayers.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\Grid.h"
#include "..\Main\MemObject.h"
#include "..\Main\gView.h"
#include "..\Main\transform.h"
#include "..\Main\BuildingInfo.h"
#include "MEParams.h"
#include "TerrainUndo.h"

extern CBasicShare<int, CMETerrainLoader> shareTerrains;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsPressed( int nKey )
{
	return 0x8000 & GetAsyncKeyState( nKey );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
bool IsTerrainLayer( ELayer el ) { return el == LID_HEIGHTS || el == LID_TILES || el == LID_GRASS || el == LID_ALPHA; }

static float brSizes[] = { 0.3f, 0.7f, 3.0f };
static vector<CObj<CMemObject> > brushes;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int FixBrushIndex( int nSizeID )
{
	return Min( (int)ARRAY_SIZE(brSizes) - 1, Max( 0, nSizeID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygTerrain::CWysiwygTerrain( NWorld::IEditorWorld *_pWorld, NGScene::IGameView *_pScene, int nTerrID ) 
: pWorld(_pWorld), pScene(_pScene), nTerrainID(nTerrID)
{
	if ( brushes.empty() )
	{
		for ( int i = 0; i < ARRAY_SIZE( brSizes ); ++i )
		{
			CMemObject* pO = *brushes.insert( brushes.end(), new CMemObject );
			pO->CreateCylinder( CVec3( 0, 0, 0.1f ), CVec3( 0, 0, 0.15f ), brSizes[i], 10, true  );
		}
	}
	//
	bModified = false;
	pActiveGrass = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMETerrainInfo* CWysiwygTerrain::GetTerrain()
{
	CDGPtr< CPtrFuncBase<CMETerrainInfo> > pLoader = shareTerrains.Get( nTerrainID );
	pLoader.Refresh();
	return pLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ELayer GetActiveLayer()
{
	ELayer eType;
	int nLayer;
	NBuilding::GetLayerID( GetUserSettings().GetActiveLayerID(), &eType, &nLayer );
	return eType;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SGrassLayer* FindGrassLayer( CMETerrainInfo *pTerr, int nLayerID )
{
	for ( int i = 0; i < pTerr->info.grass.size(); ++i )
		if ( nLayerID == pTerr->info.grass[i].nID )
			return &pTerr->info.grass[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::OnLButtonDown( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal, const CVec2 &ptCursor )
{
	bLBDown = true;
	ptLBDownCursor = ptCursor;

	ELayer eType;
	int nLayer;
	NBuilding::GetLayerID( GetUserSettings().GetActiveLayerID(), &eType, &nLayer );
	EEditMode eMode = GetUserSettings().GetMode();

	pUndo = new CTerrainUndo;
	if ( eType == LID_GRASS )
		pUndo->SetGrassLayer( nLayer );

	if ( EM_SELECT == eMode && eType == LID_GRASS && (0x8000 & GetAsyncKeyState( VK_CONTROL )) )
	{
		CMETerrainInfo *pTerr = GetTerrain();
		if ( pTerr )
		{
			SGrassLayer *p = FindGrassLayer( pTerr, nLayer );
			if ( p )
			{
				p->blades.push_back( FP_INV_GRID_STEP * ptPos );
				pUndo->PushBlade( true, FP_INV_GRID_STEP * ptPos );
				bModified = true;
				InvalidateGrass(  ptPos );
			}
		}
	}
	else if ( eType == LID_HEIGHTS )
	{
		pHeightEditor = CreateSphereEditor( this, FP_INV_GRID_STEP * ptPos );
	}
	else
		OnMouseMove( ptPos, ptCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::OnMouseMove( const CVec2 &ptPos, const CVec2 &ptCursor )
{
	if ( !bLBDown )
		return;
	if ( !IsPressed( VK_LBUTTON ) )
	{
		OnLButtonUp( ptPos );
		return;
	}

	const IUserSettings &settings = GetUserSettings();

	ELayer eType;
	int nLayer;
	NBuilding::GetLayerID( settings.GetActiveLayerID(), &eType, &nLayer );
	EEditMode eMode = settings.GetMode();
	if ( eMode != EM_SELECT && eMode != EM_ERASE )
		return;

	if ( IsValid( pHeightEditor ) )
	{
		pHeightEditor->OnMove( ptCursor - ptLBDownCursor );
		return;
	}

	CVec2 ptTilePos = FP_INV_GRID_STEP * ptPos;
	CMETerrainInfo *pTerr = GetTerrain();
	if ( !pTerr || ptPos == CVec2( PT_INVALID.x, PT_INVALID.y ) 
		|| ptTilePos.x < 0 || ptTilePos.x > pTerr->info.nWidth || ptTilePos.y < 0 || ptTilePos.y > pTerr->info.nHeight )
		return;
	const int nSize = FixBrushIndex( settings.GetBrushSize() );

	if ( EM_SELECT == eMode )
	{
		switch ( eType )
		{
			case  LID_HEIGHTS:
				{
					/*
					pTerr->info.heightMap[int(FP_INV_GRID_STEP * ptPos.y)][int(FP_INV_GRID_STEP * ptPos.x)] = settings.GetActiveFloor() * NBuilding::WALL_HEIGHT;
					CDGPtr<CFuncBase<STerrainInfo> > p = pWorld->GetEditableTerrain();
					p.Refresh();
					const STerrainInfo *pInfo = &p->GetValue();
					const_cast<STerrainInfo*>( pInfo )->heightMap = pTerr->info.heightMap;
					pWorld->UpdateTerrain( CTRect<float>( ptPos.x, ptPos.y, ptPos.x, ptPos.y ) );
					*/
					break;
				}
			case LID_GRASS:
				PaintGrass( nLayer, ptTilePos, brSizes[nSize], GetUserSettings().GetParam( ME_GRASS_DENSITY ) );
				break;
			case LID_TILES:
				{
					int nTex = GetUserSettings().GetSelectedBrushID( NBuilding::MakeFragmentID( LID_TILES, 0 ) );
					if ( nTex < 0 )
						break;
					PaintCircle( &CWysiwygTerrain::SetTile, ptTilePos, brSizes[nSize], nTex );
				}
				break;
		}
	} 
	else if ( EM_ERASE == eMode )
	{
		switch( eType )
		{
			case LID_GRASS:
				EraseGrass( nLayer, ptTilePos, brSizes[FixBrushIndex( settings.GetBrushSize() )] );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::OnLButtonUp( const CVec2 &ptPos )
{
	bLBDown = false;
	if ( bModified )
	{
		if ( IsValid( pHeightEditor ) && IsValid( pUndo ) )
			pHeightEditor->FillUndoInfo( pUndo );
		Serialize();
	}
	pHeightEditor = 0;
	if ( IsValid( pUndo ) )
	{
		ASSERT( pUndo->IsEmpty() != bModified );
		if ( !pUndo->IsEmpty() )
			NMapEditor::PushUndoCmd( pUndo );
		pUndo = 0;
	}
	bModified = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::Erase( const CVec2 &ptPos )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWysiwygTerrain::Update( const CVec3 &ptCross, CObjectBase *pObj, int nUserID )
{
	renderParts.clear();
	lines.clear();

	if ( ptCross == PT_INVALID )
		return false;

	const IUserSettings &user = GetUserSettings();
	const ELayer eLayer = GetActiveLayer();
	const EEditMode eMode = user.GetMode();
	const int nSize = FixBrushIndex( user.GetBrushSize() );

	switch ( eLayer )
	{
		case LID_GRASS:
			if ( eMode == EM_ERASE )
				renderParts.push_back( pScene->CreateMesh( brushes[nSize], CVec4( 0.9f, 0.4f, 0.35f, 0.8f ), MakeTransform( ptCross ) ) );
			else if ( eMode == EM_SELECT )
			{
				CVec3 pt( ptCross );
				pt.x = FP_GRID_STEP * Float2Int( pt.x * FP_INV_GRID_STEP );
				pt.y = FP_GRID_STEP * Float2Int( pt.y * FP_INV_GRID_STEP );
				pt.z = pt.z + 0.2f;
				DrawBrush( MakeTransform( pt ), brSizes[nSize], CVec3(0,0,1), 6 );
			}
			break;
		case LID_TILES:
			if ( eMode == EM_SELECT )
			{
				CVec3 pt( ptCross );
				pt.x = FP_GRID_STEP * Float2Int( pt.x * FP_INV_GRID_STEP );
				pt.y = FP_GRID_STEP * Float2Int( pt.y * FP_INV_GRID_STEP );
				pt.z = pt.z + 0.2f;
				DrawBrush( MakeTransform( pt ), brSizes[nSize], CVec3(0,0,1), 6 );
			}
			break;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::InvalidateTexture( const CTRect<float> &r )
{
	pWorld->InvalidateTerrainTextureRect( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::InvalidateTexture( const CVec2 &pt )
{
	pWorld->InvalidateTerrainTextureRect( CTRect<float>( Max( 0.0f, pt.x - 1 ), Max( 0.0f, pt.y - 1 ), pt.x + 1, pt.y + 1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::InvalidateGeometry( const CTRect<float> &r )
{
	pWorld->InvalidateTerrainGeometryRect( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::InvalidateGrass( const CTRect<float> &r )
{
	pWorld->InvalidateTerrainGrassRect( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::InvalidateGrass( const CVec2 &pt )
{
	pWorld->InvalidateTerrainGrassRect( CTRect<float>( Max( 0.0f, pt.x - 1 ), Max( 0.0f, pt.y - 1 ), pt.x + 1, pt.y + 1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::Serialize()
{
	CMETerrainInfo *pTerr = GetTerrain();
	if ( pTerr )
		SerializeTerrain( pTerr, nTerrainID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::PaintGrass( int nLayerID, const CVec2 &ptTilePos, float fRadius, float fDensity )
{
	CMETerrainInfo *pTerr = GetTerrain();
	//
	pActiveGrass = FindGrassLayer( pTerr, nLayerID );
	if ( !pActiveGrass )
		return;
	SGrassLayer &grass = *pActiveGrass;
	//
	if ( !(0x8000 & GetAsyncKeyState( VK_CONTROL )) )
		PaintCircle(&CWysiwygTerrain::SetGrass, ptTilePos, fRadius, fDensity );
	pActiveGrass = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::EraseGrass( int nLayerID, const CVec2 &ptTilePos, float fRadius )
{
	CMETerrainInfo *pTerr = GetTerrain();

	for ( int i = 0; i < pTerr->info.grass.size(); ++i )
	{
		SGrassLayer &grass = pTerr->info.grass[i];
		if ( nLayerID == grass.nID )
		{
			PaintGrass( nLayerID, ptTilePos, fRadius, 0 );
			for ( vector<CVec2>::iterator it = grass.blades.begin(); it != grass.blades.end(); )
			{
				if ( fabs( ptTilePos - *it ) < FP_INV_GRID_STEP * brSizes[FixBrushIndex( GetUserSettings().GetBrushSize() )] )
				{
					CVec2 pt = FP_GRID_STEP * *it;
					InvalidateGrass( pt );
					if ( IsValid( pUndo ) )
						pUndo->PushBlade( false, *it );
					it = grass.blades.erase( it );
					bModified = true;
				}
				else
					++it;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::SetTile( CMETerrainInfo *pInfo, int nx, int ny, float val )
{
	if ( nx < 0 || ny < 0 || nx >= pInfo->info.typeMap.GetXSize() || ny >= pInfo->info.typeMap.GetYSize() )
		return;
	const unsigned char oldVal = pInfo->info.typeMap[ny][nx];
	if ( oldVal == val )
		return;
	pInfo->info.typeMap[ny][nx] = val;
	if ( IsValid( pUndo ) )
		pUndo->PushTextureOp( STextureOp( nx, ny, oldVal, val ) );
	InvalidateTexture( CVec2( nx, ny ) * FP_GRID_STEP );
	bModified = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CWysiwygTerrain::SetGrass( CMETerrainInfo *pInfo, int nx, int ny, float val )
{
	if ( !pActiveGrass )
	{
		ASSERT(0);
		return;
	}
	CArray2D<BYTE> &grass = pActiveGrass->grass;
	if ( nx < 0 || ny < 0 || nx >= grass.GetXSize() || ny >= grass.GetYSize() )
		return;
	int nDensity = val;
	int oldDensity = grass[ny][nx];
	if ( oldDensity == nDensity )
		return;
	grass[ny][nx] = nDensity;
	if ( IsValid( pUndo ) )
		pUndo->PushGrassOp( SGrassOp( nx, ny, oldDensity, nDensity ) );
	InvalidateGrass( CVec2( nx, ny ) * FP_GRID_STEP );
	bModified = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::PaintCircle( FSetPoint pFunc, const CVec2 &ptTilePos, float fRadius, float fValue )
{
	CMETerrainInfo *pTerr = GetTerrain();

	int nx = Float2Int( ptTilePos.x );
	int ny = Float2Int( ptTilePos.y );
	int nRadius = Float2Int( FP_INV_GRID_STEP * fRadius );
	if ( nRadius < 1 )
	{
		(this->*pFunc)( pTerr, nx, ny, fValue );
		return;
	}
	//
	for ( int x = nx - nRadius; x <= nx + nRadius; ++x )
		for ( int y = ny - nRadius; y <= ny + nRadius; ++y )
			if ( sqr( nx - x ) + sqr( ny - y ) <= nRadius * nRadius )
				(this->*pFunc)( pTerr, x, y, fValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::DrawBrush( const SFBTransform &pos, float fRadius, const CVec3 &color, int nSegs )
{
	vector<CVec3> points;

	const float fStep = FP_PI2 / nSegs;
	for ( float f = 0; f < FP_PI2; f += fStep )
		points.push_back( CVec3( fRadius * cos( f ), fRadius * sin( f ), 0 ) );
	for ( int i = points.size() - 1; i >= 0; --i )
	{
		const CVec3 &pt = points[i];
		points.push_back( CVec3( -pt.x, pt.y, pt.z ) );
	}
	for ( int i = points.size() - 1; i >= 0; --i )
	{
		const CVec3 &pt = points[i];
		points.push_back( CVec3( pt.x, -pt.y, pt.z ) );
	}
	for ( int i = 0; i < points.size(); ++i )
		pos.forward.RotateHVector( &points[i], points[i] );
	lines.push_back( pScene->CreatePolyline( points, color ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CWysiwygTerrain::GetBaseHeight()
{
	CMETerrainInfo *pT = GetTerrain();
	if ( !pT )
		return 0;
	return pT->fMinH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygTerrain::Cancel()
{
	if ( IsValid( pHeightEditor ) )
		pHeightEditor->Cancel();
	OnLButtonUp( CVec2(0,0) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
