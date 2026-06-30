#ifndef __WYSIWYGTERRSPOT_H_
#define __WYSIWYGTERRSPOT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygSpotSel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CRndTerrainSpot;
}
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrSpot: public CSpot
{
	OBJECT_BASIC_METHODS(CTerrSpot)
	NBuilding::SProjectedSpot spot;
	NBuilding::SProjectedSpot spotStartMove;
	CPtr<NDb::CRndTerrainSpot> pDBSpot;
	CObj<NDb::CRndTerrainSpot> pRollback;
	int nSpotIndex;

protected:
	virtual NBuilding::SProjectedSpot* GetSpot( int nUserID ) const;
	
public:
	CTerrSpot() {ASSERT(0);}
	CTerrSpot( NDb::CRndTerrainSpot *pSpot, ISelection *pSel, int nUserID );

	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool EndMove( bool bCancel );
	virtual bool Rotate( float fDRotation );
	virtual bool RotateAround( float fRotation, const CVec2 &ptCenter );
	bool Delete();
	bool DelayedDelete();
	bool GetInfo( SSelectedInfo *pInfo );
	virtual void ProcessEvent( const string &str );
	virtual void OnCopy();
	virtual bool IsEqual( int nID ) const;
	virtual int  GetSelectionID() const;
	virtual void Move( const CVec3 &ptMove );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGTERRSPOT_H_
