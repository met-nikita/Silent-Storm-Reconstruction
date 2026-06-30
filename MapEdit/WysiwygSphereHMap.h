#ifndef __SPHEREHMAPEDIT_H_
#define __SPHEREHMAPEDIT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "WysiwygTerrain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSphereHeightMap: public IHeightEditor
{
	OBJECT_BASIC_METHODS(CSphereHeightMap)
	CPtr<CWysiwygTerrain> pTerrain;
	CVec2 ptWorldCenter;
	CTPoint<int> ptCenter;
	bool bValid;
	CArray2D<unsigned short> hm;
	float fLastHDelta;
	float fLastRadius;
	CTRect<float> rTotal;
	bool bLastRectValid;
	bool bTotalValid;
	CTRect<float> rLast;

	CTRect<float> MakeSphere( float fRadius, float fHeight );
	void UpdateTotalRect( const CTRect<float> &r );

public:
	CSphereHeightMap() {}
	CSphereHeightMap( CWysiwygTerrain *pTerr, const CVec2 &ptTerrainPos );

	virtual void OnMove( const CVec2 &ptDelta );
	virtual void Cancel();
	virtual void FillUndoInfo( CTerrainUndo *pUndo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SPHEREHMAPEDIT_H_
