#ifndef __BUILDINGSCHEMA_H_
#define __BUILDINGSCHEMA_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RodJunction.h"
namespace NDb
{
	class CRPGArmor;
}
namespace NBuilding
{
class CBuildingGrid;
struct SPoint3;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SJunctionHash
{
	int operator() ( const CIVec3 &pt ) const { return pt.z << 24 | pt.y << 16 | pt.x; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<CIVec3, CJunctionID, SJunctionHash> CJunctionHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroundHash // z не учитывается
{
	int operator() ( const CTPoint<int> &pt ) const { return pt.y << 16 | pt.x; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<CTPoint<int>, CJunctionID, SGroundHash> CGroundHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SJunction
{
	CVec3 pt;
	bool  bGround;

	SJunction( const CVec3 &_pt, bool _bGr = false ): pt(_pt), bGround(_bGr) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildingSchema: public CObjectBase
{
	OBJECT_BASIC_METHODS(CBuildingSchema);

	//list< CObj<CJunction> > juncs;
	//list< CObj<CRod> > rods;
	vector<CJunction> juncs;
	vector<int> sortedJuncs;
	vector<CRod> rods;
	CJunctionHash junchash;
	CGroundHash groundhash;
	vector<SPath> waves;

	void Clear();
	//
	void ComputeStability();
	void ComputeStabilityR();
	void FindFree();
	void ComputeMoments();

	bool Destroy( CBuildingGrid *pGrid, CJunction *pJ, int nDepth = 0 );
	void CheckGroundHash( CJunction *pJ );
	void CheckArtifacts();

public:
	CBuildingSchema();

	CRod* AddRod( NDb::CRPGArmor *pArmor, SRodEdge ptLeft, SRodEdge ptRight, bool bCellarWall, bool bGround );

	void AddNode( NDb::CRPGArmor *pArmor, const CVec3 &pt, bool bCellarWall, float fWeight, const vector<SJunction> &points, bool bFilled = true );
	bool Recalc( CBuildingGrid *pGrid );

	void Destroy(	CBuildingGrid *pGrid, const SPoint3 &pt );

	void Reset();
	void Start(); // сортирует узлы
	void Reserve( int nJuncs, int nRods );

	// функции используемые при визуализации
	const vector<CRod>& GetRods() const { return rods; }
	const vector<CJunction>& GetJuncs() const { return juncs; }

	//
	bool IsJunctionValid( CJunctionID nID ) const
	{
		return nID >= 0 && nID < juncs.size() && !juncs[nID].IsDestroyed();
	}
	bool IsRodValid( CRodID nID ) const
	{
		return nID >= 0 && nID < rods.size() && !rods[nID].IsDestroyed();
	}
	CJunction* GetJunction( CJunctionID nID )
	{
		ASSERT( IsJunctionValid( nID ) );
		return &juncs[nID];
	}
	CRod* GetRod( CRodID nID )
	{
		ASSERT( IsRodValid( nID ) );
		return &rods[nID];
	}
	vector<SPath>* GetWaves()
	{
		return &waves;
	}
	const CJunction* GetJunction( CJunctionID nID ) const {	ASSERT( IsJunctionValid( nID ) );	return &juncs[nID];	}
	const CRod* GetRod( CRodID nID ) const { ASSERT( IsRodValid( nID ) );	return &rods[nID]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BUILDINGSCHEMA_H_
