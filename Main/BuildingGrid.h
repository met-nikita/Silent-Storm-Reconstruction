#ifndef __BUILDINGGRID_H_
#define __BUILDINGGRID_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DG.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\2DArray.h"
#include "BuildingPart.h"

namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPoint3
{
	int x, y, z;
	SPoint3() {}
	SPoint3( int _x, int _y, int _z ) : x( _x ), y( _y ), z( _z ) {}
	SPoint3( const CVec3 &pt ) : x( Float2Int(pt.x) ), y( Float2Int(pt.y) ), z( Float2Int(pt.z) ) {}
};
inline SPoint3 operator+( const SPoint3 &a, const SPoint3 &b ) { return SPoint3(a.x + b.x, a.y + b.y, a.z + b.z); }
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRoomMatch
{
	int nFloor, nInternalRoom, nGlobalRoom;

	SRoomMatch() {}
	SRoomMatch( int _nFloor, int _nInt, int _nGlob )
		: nFloor(_nFloor), nInternalRoom(_nInt), nGlobalRoom(_nGlob) {}
};
const int ZSHIFT = 16;
inline int GetPieceHash( const SPoint3 &p ) 
{
	const int x = p.x + 4;
	const int y = p.y + 4;
	const int z = p.z + ZSHIFT + 4;
	ASSERT( (x & 0xffffff00)==0 );
	ASSERT( (y & 0xffffff00)==0 );
	ASSERT( (z & 0xffffff00)==0 );
	return (z << 16) | (y << 8) | x; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void GetPieceHashCoords( int nHash, SPoint3 *p ) 
{ 
	p->z = ((nHash>>16) & 0xff) - ZSHIFT - 4;
	p->y = ((nHash>>8) & 0xff) - 4; 
	p->x = ((nHash)&0xff) - 4; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildingSchema;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildingGrid : public CVersioningBase
{
	OBJECT_BASIC_METHODS( CBuildingGrid );
	enum { DESTROY_LIM = 255 };
	typedef SPlane SPlane6[6];

	ZDATA
	CArray3D<BYTE> net;
	int nDZ;
	CVec3 ptBoxMin;
	CVec3 ptBoxMax;
	SFBTransform pos;
	vector<NBuilding::SRoomMatch> rooms;
	SRandomSeed seed;
	bool bStabilityUpdate;
	int nBaseFloor;
	
	// computed values /ComputeAuxValues()/
	SPlane6 box;
	int nCutFloor; // for WYSIWYG
	unordered_map<int, bool> visibleLayers;
	bool bOnlyCutFloorVisible;
	unordered_map<SPart, bool, SPart> updatedParts;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&net); f.Add(3,&nDZ); f.Add(4,&ptBoxMin); f.Add(5,&ptBoxMax); f.Add(6,&pos); f.Add(7,&rooms); f.Add(8,&seed); f.Add(9,&bStabilityUpdate); f.Add(10,&nBaseFloor); f.Add(11,&box); f.Add(12,&nCutFloor); f.Add(13,&visibleLayers); f.Add(14,&bOnlyCutFloorVisible); f.Add(15,&updatedParts); return 0; }
	CObj<CBuildingSchema> pSchema;
	vector<SPoint3> brokenSpots;   // retail @0xf8: transient per-flush list of destroyed voxels (FX source); NOT serialized

	void ComputeAuxValues();
	bool IsValidCoord( const SPoint3 &pt ) const;
	BYTE& At( const SPoint3 &pt );
	bool Intersection( const CVec3 &ptCenter, float fRadius, CVec3 *pPtMin, CVec3 *pPtMax );
	SPart Point2Part( const SPoint3 &pt );

public:
	
	CBuildingGrid();
	void AddRoom( int nFloor, int nInternal, int Global );
	int GetRoomGlobal( int nFloor, int nInternal ) const;
	void GetRoomLocal( int nGlobalID, int *pnFloor, int *pnInternal ) const;
	void Setup( int nMaxX, int nMaxY, int nMinFloor, int nMaxFloor, const CVec2 &ptMinXY, const SFBTransform &pos );
	bool DamageSpot( const SPoint3 &pt, int nDmg = DESTROY_LIM, bool bAddToBrokenSpots = false );
	bool IsDestroyed( const SPoint3 &pt ) const;
	void Explode( const CVec3 &ptEpicentre, int nPower, float fRadius );
	const SRandomSeed& GetSeed() const { return seed; }
	void GetSize( CVec3 *pptMin, CVec3 *pptMax ) const;
	SFBTransform GetPos() const { return pos; }
	void SetPos( const SFBTransform &_pos ) { pos = _pos; }
	void Reset();
	void ToggleStability() { bStabilityUpdate = !bStabilityUpdate; }
	bool NeedComputeStability() const { return bStabilityUpdate; }
	void AddHP( const SPoint3 &pt, BYTE hp );
	void SetIndestructible( const SPoint3 &pt );
	void SetCellar( const SPoint3 &pt );
	bool IsCellar( const SPoint3 &pt );
	void SetBaseFloor( int nBaseF ) { nBaseFloor = nBaseF; }
	int  GetBaseFloor() const { return nBaseFloor; }
	void SetCutFloor( int nFloor );
	int  GetCutFloor() const { return nCutFloor; }
	// building visualization by layers (MapEditor)
	void SetVisibleLayers( const vector<int> &layers );
	void SetOnlyCutFloorVisible( bool bVis );
	bool IsLayerVisible( int nLayerID ) const;
	bool IsOnlyCutFloorVisible() const;
	// building parts
	void UpdatePart( const SPoint3 &pt );
	void GetUpdatedParts( vector<SPart> *pParts );
	void GetBrokenSpots( vector<SPoint3> *pSpots );   // retail @0xc3170: swaps the destroyed-voxel list out to the caller

	CBuildingSchema* GetSchema() const { return pSchema; }
	void SetSchema( CBuildingSchema *p ) { pSchema = p; }
	int  GetHP( const SPoint3 &pt ) const { return const_cast<CBuildingGrid*>(this)->At( pt ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __BUILDINGGRID_H_