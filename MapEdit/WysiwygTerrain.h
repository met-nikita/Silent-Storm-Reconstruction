#ifndef __IWYSIWYGTERRAIN_H_
#define __IWYSIWYGTERRAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Main\MELayers.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class IEditorWorld;
}
namespace NGScene
{
	class IGameView;
	class CPolyline;
}
class CMETerrainInfo;
struct SGrassLayer;
class CTerrainUndo;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float TERR_ERASE_RADIUS = 1;
const CVec3 PT_INVALID( -1, -1, -1 );
class CWysiwygTerrain;
////////////////////////////////////////////////////////////////////////////////////////////////////
class IHeightEditor: public CObjectBase
{
public:
	virtual void OnMove( const CVec2 &ptDelta ) = 0;
	virtual void Cancel() = 0;
	virtual void FillUndoInfo( CTerrainUndo *pUndo ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygTerrain: public CObjectBase
{
	OBJECT_BASIC_METHODS(CWysiwygTerrain);
	int nTerrainID;
	NWorld::IEditorWorld *pWorld;
	CPtr<NGScene::IGameView> pScene;
	bool bLBDown;
	vector<CObj<CObjectBase> > renderParts;
	list< CObj<NGScene::CPolyline> > lines;
	bool bModified;
	SGrassLayer *pActiveGrass;
	CObj<CTerrainUndo> pUndo;
	CObj<IHeightEditor> pHeightEditor;
	CVec2 ptLBDownCursor;

protected:
	CMETerrainInfo* GetTerrain();
	void PaintGrass( int nLayerID, const CVec2 &ptTilePos, float fRadius, float fDensity );
	void EraseGrass( int nLayerID, const CVec2 &ptTilePos, float fRadius );
	void InvalidateTexture( const CTRect<float> &r );
	void InvalidateTexture( const CVec2 &pt );
	void InvalidateGeometry( const CTRect<float> &r );
	void InvalidateGrass( const CTRect<float> &r );
	void InvalidateGrass( const CVec2 &pt );
	void Serialize();
	void DrawBrush( const SFBTransform &pos, float fRadius, const CVec3 &color, int nSegs );
	void SetTile( CMETerrainInfo *pInfo, int nx, int ny, float val );
	void SetGrass( CMETerrainInfo *pInfo, int nx, int ny, float val );
	typedef void (CWysiwygTerrain::*FSetPoint)( CMETerrainInfo *pInfo, int nx, int ny, float fVal );
	void PaintCircle( FSetPoint pFunc, const CVec2 &ptTilePos, float fRadius, float fValue );
	void SetModified() { bModified = true; }

	friend class CSphereHeightMap;

public:
	CWysiwygTerrain() {}
	CWysiwygTerrain( NWorld::IEditorWorld *pWorld, NGScene::IGameView *pScene, int nTerrainID );

	void OnLButtonDown( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal, const CVec2 &ptCursor );
	void OnMouseMove( const CVec2 &ptPos, const CVec2 &ptCursor );
	void OnLButtonUp( const CVec2 &ptPos );
	void Erase( const CVec2 &ptPos );
	bool Update( const CVec3 &ptCross, CObjectBase *pObj, int nUserID );
	void Cancel();

	float GetBaseHeight();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IHeightEditor* CreateSphereEditor( CWysiwygTerrain *pTerr, const CVec2 &ptTilePos );
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsTerrainLayer( ELayer el );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IWYSIWYGTERRAIN_H_
