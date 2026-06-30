#ifndef __TERRAINUNDO_H_
#define __TERRAINUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygUndo.h"
#include "..\Misc\2Darray.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCoord, class TValue>
struct SOperation
{
	TCoord x, y;
	TValue oldValue;
	TValue newValue;

	SOperation() {}
	SOperation( const TCoord _x, const TCoord _y, const TValue oldv, const TValue newv  )
		:x(_x), y(_y), oldValue(oldv), newValue(newv) {}
};
typedef SOperation<short, unsigned char> STextureOp;
typedef SOperation<short, BYTE> SGrassOp;
typedef SOperation<CTRect<float>, CArray2D<unsigned short> > SHeightMapOp;
class CMETerrainInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainUndo: public CWysiwygUndo
{
	OBJECT_NOCOPY_METHODS(CTerrainUndo)
private:
	vector<STextureOp> texture;
	int nGrassLayer;
	vector<SGrassOp> grass;
	vector<CVec2> bladesInserted;
	vector<CVec2> bladesDeleted;
	vector<SHeightMapOp> hm;
	bool bEmpty;

	CMETerrainInfo* GetTerrain();

public:
	CTerrainUndo();

	virtual bool DoUndo( NMapEditor::CPostProcessQueue *pQueue );
	virtual bool DoRedo( NMapEditor::CPostProcessQueue *pQueue );

	bool IsEmpty() const { return bEmpty; }
	void SetGrassLayer( int nLayerID );
	void PushTextureOp( const STextureOp &op );
	void PushGrassOp( const SGrassOp &op );
	void PushBlade( bool bInsert, const CVec2 &pt );
	void SetHeightMapOp( const CArray2D<unsigned short> &oldhm, const CArray2D<unsigned short> &newhm, const CTRect<float> &rUpdate );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TERRAINUNDO_H_
