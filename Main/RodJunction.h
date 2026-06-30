#ifndef __RODJUNCTION_H_
#define __RODJUNCTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "rod.h"
struct SRand;
namespace NDb
{
	class CRPGArmor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
class CBuildingSchema;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EDirection
{
	XP = 0, // +x
	XM = 1, // -x
	YP = 2, // +y
	YM = 3, // -y
	UP = 4,
	DN = 5,
	NDIRECTIONS = 6,
	XPYP = 7,
	XMYP = 8,
	XMYM = 9,
	XPYM = 10,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRodEdge
{
	CIVec3 pt;
	bool bFilled;
	float fWeight;

	SRodEdge( const CVec3 &point, bool bFilledNode, float _fWeight )
		: pt(point.x, point.y, point.z ), bFilled(bFilledNode ), fWeight(_fWeight)
	{
	}
	bool operator==( const SRodEdge &op ) const { return pt == op.pt && bFilled == op.bFilled; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRod;
enum ERodSide;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPath
{
	CJunction *pJ;
	float fWeight;
	EDirection from;
	SPath(): pJ(0) {}
	SPath( CJunction *p, float fW, EDirection fr ) : pJ( p ), fWeight( fW ), from( fr ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNeighb
{
	CRodID nRod;
	ERodSide side;				// край pRod, который является соседом для текущей точки

	SNeighb() : nRod(-1) {}
	SNeighb( CRodID nR, ERodSide s ) : nRod(nR), side(s) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CJunction
{
public:
	enum EStability 
	{ 
		STABLE,	
		UNSTABLE,		// может отвалиться, висит в воздухе или нет - неизвестно
		FREE,				// висит в воздухе
		UNSTABLE_NOTFREE, // точно не висит в воздухе
		UNKNOWN, 
		UNINITIALIZED,
		UNKNOWN_MOMENT,	// в этой точке не удалось посчитать момент
	};

private:
	typedef list< pair<SNeighb, EDirection> > CNeighbList;
	typedef vector< pair<SNeighb, EDirection> > CNeighbVec;
	const int nID;
	CBuildingSchema *const pSchema;
	SNeighb neighbs[NDIRECTIONS];
	//CNeighbList nlist;
	EStability stability;
	//EStability stabilities[NDIRECTIONS];	// стабильность по разным направлениям
	float fJWeight;						// масса узла
	float moments[UP];				// посчитанные моменты 
	float fPressure;					// общая масса, давящая на узел
	float fUltimateMoment;		// макс. момент, выдерживаемый узлом
	float fUltimatePressure;	// макс. давление
	int   nFlags;
	int   nNeighbs;
	//bool  bArrows[XPYM + 1];	// "стрелы" попавшие в узел из стабильных узлов
	enum EFlags
	{
		FLAG_GROUND     = 1 << 16,		// узел находится на земле <=> разрушаем или нет данный узел
		FLAG_CELLARWALL = 1 << 17,		// принадлежит стенке подвала => неразрушаемый узел
		FLAG_LOCK       = 1 << 18,
		FLAG_FREECHECK  = 1 << 19,
		FLAG_FILLED     = 1 << 20,		// соответсвует ли узел строительному блоку
		FLAG_DESTROY    = 1 << 21,
		FLAG_WAVEFRONT  = 1 << 22,
		FLAG_BOTTOM     = 1 << 23,
	};
	//
	vector<SPath> *const pWaves;
	vector<int>	stableJunctions; // 
	//vector<SPath> stableJunctions;
	float fStableWeight;

	vector<CJunctionID> parents; // ближайший узлы, который соответсвует строительному блоку
	EDirection wfrom;
	//SPath wavefront;
	int wavefront;
	int   nIteration;

	bool Flag( EFlags eFlag ) const { return nFlags & eFlag; }
	void SetFlag( EFlags eFlag ) { nFlags |= eFlag; }
	void ClearFlag( EFlags eFlag ) { nFlags &= ~eFlag; }
	bool Arrow( const EDirection dir ) const;
	void SetArrow( const EDirection dir );
	bool CheckStability();
	//EStability RecurseCheckStability( EDirection dir );
	bool Lock() { if ( Flag( FLAG_LOCK ) ) return false; SetFlag( FLAG_LOCK ); return true; }
	void Unlock() { ClearFlag( FLAG_LOCK ); }
	void AddMoment( float fWeight, const CIVec3 &ptMass, const EDirection from );
	void AdvanceWavefront( const SPath &stableJ );
	void Init();

	friend class CRod;
	
public:
	const CIVec3 ptJ;
public:
	CJunction( CBuildingSchema *pSchema, int nID, const SRodEdge &pt, float fWeight, bool bGround, bool bCellarWall );

	int GetID() const { return nID; }
	void AddNeighbour( const SNeighb &neighb );
	void AddWeight( float fWeight ) { fJWeight += fWeight; }
	bool Shoot( EDirection dir );
	EStability GetStability() const { return stability; }
	void UpdateVerticalStability();
	//EStability RecurseCheckStability();
	EStability CheckFree( list<CJunction*> *pDomain, int nDepth = 0 );
	bool HasRightAngle();
	void SetStability( EStability s ) { stability = s; }
	bool IsFreeCheck() const { return Flag( FLAG_FREECHECK ); }
	void ComputeMoment();
	void ComputeStableJInfluence( CJuncList *pWaveList );
	float GetWeight() const { return fJWeight; }
	float GetPressure() const { return fPressure; }
	float GetMoment( EDirection dir ) const;
	float GetMoment( const CRod *pRod ) const; // для визуализации
	CJunctionID GetNeighbour( EDirection dir ) const;
	CRodID GetRod( EDirection dir ) const;
	bool  IsFilled() const { return Flag( FLAG_FILLED ); }
	void  SetFilled() { SetFlag( FLAG_FILLED ); }
	bool IsWaveFrontPoint() const { return Flag( FLAG_WAVEFRONT ); }
	bool ProcessWavefront( SRand *pRand, int nStep, CJuncList *pWaveList );
	CJunction* GetNearestFilled() const;
	bool IsGround() const { return Flag( FLAG_GROUND ); }
	bool IsCellarWall() const { return Flag( FLAG_CELLARWALL ); }
	void SetGround( bool bGround = true );
	void SetDestroyLimits( NDb::CRPGArmor *pArmor );
	bool IsBroken( int nJuncHP ) const;
	bool IsLinkBroken( EDirection dir ) const;
	bool IsLinkBroken( const CRod *pRod ) const;
	void Destroy();
	bool IsDestroyed() const { return Flag( FLAG_DESTROY ); }
	void Reset();
	void AddParent( CJunctionID nJ );
	void DestroyRod( EDirection dir );
	void DestroyRod( CJunction *pJ );
	int  CountLinks() const;
	void CheckVerticalValidity();
	bool IsBottom() const { return Flag( FLAG_BOTTOM ); }
	void SetBottom( bool bBottom );

	void operator=( const CJunction &op )
	{
		memcpy( this, &op, sizeof( *this ) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float CJunction::GetMoment( EDirection dir ) const 
{ 
	if ( dir < UP ) 
		return moments[dir];
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RODJUNCTION_H_
