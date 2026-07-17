#ifndef __aiGrid_H_
#define __aiGrid_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiPosition.h"

namespace NAI
{
enum ECheckPose
{
	CP_INACTIVE = 0x01, // can't act; can only stand still and not fidget
	CP_LAY1     = 0x02,
	CP_LAY2     = 0x04,
	CP_LAY3     = 0x08,
	CP_LAY4     = 0x10,
	CP_LAY      = 0x20,
	CP_CROUCH   = 0x40,
	CP_STAND    = 0x80
};
enum ELadderDirection
{
	LD_RIGHT = 0, 
	LD_FRONT = 1, 
	LD_LEFT = 2, 
	LD_BACK = 3
};
enum ECheckDirection
{
	CD_R,
	CD_RU,
	CD_U,
	CD_LU,
	CD_L,
	CD_LD,
	CD_D,
	CD_RD,
	CD_LAST
};
enum ETileFlags
{
	TF_STAND_PASSABLE = 1,
	TF_HAS_INTERGRID = 2,
	TF_HAS_SAME = 4,
	TF_IS_LADDER_UP = 8,
};
// limits for climbing/jumping
const float F_MIN_CLIMB_HEIGHT = 0.55f;//0.36f;
const float F_MAX_CLIMB_HEIGHT = 2.8f;
//! vertical distance between ladder points
const float F_LADDER_STEP = 0.625f;
//! vertical quantum of a "3D"/fly position -- a fly place repurposes its nLayer (8 bits) as the
//! altitude above its ground (layer-0) cell, in F_3D_STEP units. Set by NAI::MakeFlyPos, decoded
//! by CPathNetwork::GetCP/GetCPNoHeight/GetDirection/GetFloor (retail @0x41d20 etc.).
const float F_3D_STEP = 0.1f;
//! limit on height range (minimum is always 0)
const float F_MAX_HEIGHT = 25.0f;
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MAX_FLOORS = 8;
// retail = 3 (disasm: RegisterFloorLayer @0x3f610 / SetOnBaseLayer @0x77ba0 `lea eax,[eax+eax*2]`,
// PDB STempArrayGroup = [8][3]). LOAD-BEARING for bTop: on a 4+-surface column (switchback
// stairwell) the 3rd slot IS the floor's top layer (bTop short-circuit) and the 4th surface
// SPILLS into the next floor's slot 0 (retail's unguarded table write, see CreateAdditionalLayers)
// -- with the old dev 4, the tile on slot 2 saw a same-floor surface above, bTop stayed false and
// the (bCollides && !bTop) gate killed steep stairwell tiles.
const int N_MAX_LAYERS_PER_FLOOR = 3;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline unsigned short GetIHeight( float fH ) { return Float2Int( fH * (65536 / F_MAX_HEIGHT) ); }
inline float GetFHeight( unsigned short nH ) { return nH * (F_MAX_HEIGHT / 65536); }
inline float GetFHeight( int nH ) { return nH * (F_MAX_HEIGHT / 65536); }
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> 
	inline bool IsInArray( const CArray2D<T> &array, int nX, int nY )
{
	return ( nX >= 0 && nY >= 0 && nX < array.GetXSize() && nY < array.GetYSize() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STile
{
	union 
	{
		struct 
		{
			char nMoveLay, nMoveCrouch, nMoveStand, nMoveHC; // move flags by direction by pose
		};
		char nMove[4];
	};
	unsigned short nHeight;
	// retail STile +6 (PDB `short nFloor`): the NATIVE SOURCE FLOOR of the surface this tile stands
	// on (terrain = 0, object = its record floor), stamped by CPassCalcer::CalcPoseInPoint from the
	// height-field's floor channel. CPathNetwork::GetFloor @0x3df00 reads it per tile -- the
	// approximate-path fallback keys on it, so a jump-landing tile on a building group's UPPER
	// LAYER still reports the landing surface's floor. (Was the never-written Jan03 `nFake`.)
	short nFloor;
	char nLocks;
	char nDynLocks;
	char nPassable; // // set of 1<<ECheckPose bits
	char nDisplacement;
	char nFlags;
	unsigned char nFlipper; 
	// (nFlipper != 0) if a tile is locked by a door or another flipping object and is a number of that flipper + 1
	char nFake1, nFake2; // needed to make structure size = 16
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathNetwork;
class CMapColourer;
class CLadderTracker;
class CNodesLayer;
class CLayersSetTracker;
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct STempArray : public CObjectBase
{
	OBJECT_BASIC_METHODS( STempArray );
public:
	CArray2D<T> content;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct STempArrayGroup
{
	typedef CArray2D<T> STemp;
	CObj< STempArray<T> > tempArrays[ N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR ];
	CTRect<int> region;
	STemp* GetArray( int nFloor, int nLayer ) 
	{
		STemp *pTemp;
		if ( !tempArrays[ nLayer + nFloor * N_MAX_LAYERS_PER_FLOOR ] )
		{
			tempArrays[ nLayer + nFloor * N_MAX_LAYERS_PER_FLOOR ] = new STempArray<T>;
			STempArray<T> *pTempA = tempArrays[ nLayer + nFloor * N_MAX_LAYERS_PER_FLOOR ];
			pTemp = &pTempA->content;
			pTemp->SetSizes( region.Width(), region.Height() );
			pTemp->FillZero();
		}
		else
		{
			STempArray<T> *pTempA = tempArrays[ nLayer + nFloor * N_MAX_LAYERS_PER_FLOOR ];
			pTemp = &pTempA->content;
		}
		return pTemp;
	}
	STemp* GetArray( int nFloor, int nLayer ) const
	{
		STempArray<T> *pTempA = tempArrays[ nLayer + nFloor * N_MAX_LAYERS_PER_FLOOR ];
		return &pTempA->content;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//	Diplacements:
//	5 1 6 
//	4 0 2  
//	8 3 7
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_DISPL = 0.2083f;
const float F_DISPLACEMENT_X[] = { 0, 0, F_DISPL, 0, -F_DISPL, -F_DISPL, F_DISPL, F_DISPL, -F_DISPL };
const float F_DISPLACEMENT_Y[] = { 0, F_DISPL, 0, -F_DISPL, 0, F_DISPL, F_DISPL, -F_DISPL, -F_DISPL };
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLayersGroup: public CObjectBase
{
	OBJECT_BASIC_METHODS(CLayersGroup);
public:
	struct SIgnoreRect
	{
		CVec2 ptOrigin, ptXDir, ptSize;

		SIgnoreRect() {}
		SIgnoreRect( const CVec2 &_ptOrigin, const CVec2 &_ptXDir, const CVec2 &_ptSize )
			:ptOrigin(_ptOrigin), ptXDir(_ptXDir), ptSize(_ptSize) {}
			bool IsInside( const CVec2 &ptTest ) const;
	};
	struct SNotYetCreatedLadder
	{
		int nX, nY;
		int nHeight;
		int nRotation;
		int nFloor;
	};
	vector<SNotYetCreatedLadder> ladders; // ladders are kept outside the ZDATA - ZEND "brackets",
	// because at serialization time this vector must always be empty.
	ZDATA
	// a group's first floor may be below 0 for groups with basements
	int nFirstFloor;
	// data shared by all layers
	CVec2 ptOrigin;
	CVec2 ptXDir;
	CArray2D<char> squareLevel;   // level of data ready for each region (0-nothing; 1-height+potential out; 2-full)
	CArray2D<CObj<CLayersSetTracker> > squareTrackers;
	CPtr<CPathNetwork> pNet;
	vector< CPtr<CNodesLayer> > layers;
	int nXSize, nYSize;
	bool bFreeze;
	vector<SIgnoreRect> ignoreRects; // rects where all points are considered not native
	int nPassCalcJobsLeft;
	bool bHasRecolourJob;
	ZEND int operator&( CStructureSaver &f ) { f.Add(3,&nFirstFloor); f.Add(4,&ptOrigin); f.Add(5,&ptXDir); f.Add(6,&squareLevel); f.Add(7,&squareTrackers); f.Add(8,&pNet); f.Add(9,&layers); f.Add(10,&nXSize); f.Add(11,&nYSize); f.Add(12,&bFreeze); f.Add(13,&ignoreRects); f.Add(14,&nPassCalcJobsLeft); f.Add(15,&bHasRecolourJob); return 0; }
	CLayersGroup() :bFreeze(false), nPassCalcJobsLeft(0), bHasRecolourJob(false) {}
	SPoint GetPoint( const CVec3 &pos ) const;
	void GetExactPoint( CVec2 *pRes, const CVec2 &pos ) const;
	bool IsInside( const CVec2 &ptTest ) const;
	int GetXSize() const { return nXSize; }
	int GetYSize() const { return nYSize; }
	void RecalcSquare( int nX, int nY, IAIMap *pMap, char nLevel );
	bool TestSquare( int nX, int nY, char nLevel );
	// retail @0x441c70 returns bool (true = a square recalc ran) and takes bForce: a pending
	// recolour job suppresses refreshes unless forced (the pass-calcer job forces, @0x488880).
	bool RefreshSpot( const SPathPlace &p, IAIMap *pMap, char cLevel, bool bForce = false );
	void SetTracker( int nX, int nY, IAIMap *pMap, const STempArrayGroup<STile> &tempArrays );
	CVec2 GetCPNoHeight( float x, float y ) const 
	{
		return CVec2( ptOrigin.x + ptXDir.x * x - ptXDir.y * y, ptOrigin.y + ptXDir.y * x + ptXDir.x * y );
	}
	float GetDirection( const SPathPlace &p ) const
	{
		float fAngle = atan2( ptXDir.y, ptXDir.x ) + p.GetDirection() * FP_PI4;
		return NormalizeAngleInRadian( fAngle );
	}
	void BuildLayersGroup( int nXSize, int nYSize, const CVec2 &_ptOrigin, const CVec2 &_ptXDir, CPathNetwork *_pNet );
	void RegisterFloorLayer( CNodesLayer *pLayer, int nFloor );
	bool IsIgnored( float x, float y ) const;
	CNodesLayer* GetRootFloorLayer( int nFloor );
	// retail @0x3e740: the layer whose TILE at (x,y) natively belongs to nFloor (passable + some
	// move connectivity); falls back to the floor's root layer
	CNodesLayer* GetLayerWithFloor( int nFloor, int nX, int nY );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNodesLayer: public CObjectBase
{
	OBJECT_BASIC_METHODS(CNodesLayer);
public:
	/*struct SSpecialPoint
	{
		CVec2 pt;
		unsigned short nHeight;
		char nPassable; // set of 1<<ECheckPose bits
		unsigned short nHGroupSet;
	};*/
	typedef NAI::STile STile;
	struct SLink
	{
		SPathPlace dst;
		int nHeight;

		SLink() {}
		SLink( const SPathPlace &p, int _nHeight ): dst(p), nHeight(_nHeight) {}
	};
	struct STransitionSet
	{
		ZDATA
		list<SLink> links;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&links); return 0; }

		list<SLink>::iterator GetLink( const SPathPlace &p )
		{
			list<SLink>::iterator i;
			for ( i = links.begin(); i != links.end(); ++i ) if ( i->dst == p ) return i;
			return i;
		}
	};
	struct SLadder
	{
		ZDATA
		vector<char> pointPassable;
		vector<char> pointOnUpperHalf;
		vector<char> nLocks;
		SPathPlace placeOnBottom; // spot to get off/onto the ladder at the bottom point
		SPathPlace placeOnTop; // spot to get off/onto the ladder at the top point
		SPathPlace upperLink;
		ELadderDirection eDir;
		CObj<CLadderTracker> pTracker;
		bool bNeedRecalc;
		bool bConsistent; // ladder is intact, i.e. fully passable from top to bottom
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pointPassable); f.Add(3,&pointOnUpperHalf); f.Add(4,&nLocks); f.Add(5,&placeOnBottom); f.Add(6,&placeOnTop); f.Add(7,&upperLink); f.Add(8,&eDir); f.Add(9,&pTracker); f.Add(10,&bNeedRecalc); f.Add(11,&bConsistent); return 0; }
		int GetHeight() const { return pointPassable.size(); }
	};
	struct SLadderTransition
	{
		bool bUpper;
		char nLadder;
		char nLayerGroup;	
	};
	typedef unordered_map<SPathPlace, STransitionSet, SPathPlaceHash > CTransitionsHash;
	typedef unordered_map<SPathPlace, SLadderTransition, SPathPlaceHash> CLadderHash;
	ZDATA
	// Layer - specific
	int nLayer;
	int nFloor;
	CArray2D<STile> tiles;        // regular grid
	CTransitionsHash transitions;
	vector<SLadder> ladders;
	CLadderHash ladderEntrances;
	CObj<CMapColourer> pColourer;
	CObj<CLayersGroup> pGroup;
	bool bHappySave;   // retail 'small save': strip the recolour cache on save (set by SetSmallSaveSize @0x3db60)
	// retail @0x46bd0: 9=pGroup, 10=bHappySave, 11=pColourer -- and a happy save writes a NULL colourer
	// chunk at 11 instead of the live cache. (Dev pre-convergence had 9=pColourer/10=pGroup.)
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&nLayer); f.Add(3,&nFloor); f.Add(4,&tiles); f.Add(6,&transitions); f.Add(7,&ladders);
		f.Add(8,&ladderEntrances); f.Add(9,&pGroup); f.Add(10,&bHappySave);
		if ( !f.IsReading() && bHappySave )
		{
			CObj<CMapColourer> pNull;
			f.Add(11,&pNull);
		}
		else
			f.Add(11,&pColourer);
		return 0;
	}
	
	void RemoveAllTransitions( const CTRect<int> &rect );
	//void RecalcHeightAndPassability( IAIMap *pMap, const CTRect<int> &region, CArray2D<STile> *pTemp );
	//void RecalcTransitions( IAIMap *pMap, const CTRect<int> &region, CArray2D<STile> *pTemp );
public:
	const CPathNetwork *GetNetwork() const { return pGroup->pNet; }
	void GetSamePoints( const SPathPlace& p, vector<SPathPlace> *pRes );
	int GetSamePoints( const SPathPlace& p, SPathPlace *places ); // quicker version: places is an array having enough memory
	bool RefreshLadder( int nLadder, IAIMap *pMap );   // retail @0x408b0: true = a recalc actually ran

	char GetLocks( int x, int y ) const { return tiles[y][x].nLocks; }
	CVec2 GetCPNoHeight( float x, float y ) const 
	{
		int nX = Min( Max( 0, (int)x ), tiles.GetXSize() - 1 ), 
			nY = Min( Max( 0, (int)y ), tiles.GetYSize() - 1 );
		STile &t = tiles[nY][nX];
		int nD = t.nDisplacement;
		return pGroup->GetCPNoHeight( x, y ) + CVec2( F_DISPLACEMENT_X[ nD ], F_DISPLACEMENT_Y[ nD ] );
	}
	CVec3 GetCP( const SPathPlace &p ) const 
	{
//		ASSERT( p.nIntegral );
		float x, y, fHeight;
		SPathPlace ptOrig;
		if ( p.IsIntegral() )
		{
			ptOrig = p;
			x =p.GetX();
			y =p.GetY();
			fHeight = 0;
		}
		else
		{
			ptOrig = ladders[ p.GetX() ].placeOnBottom;
			x = ptOrig.GetX();
			y = ptOrig.GetY();
			fHeight = p.GetY() * F_LADDER_STEP;
		}
		if ( ptOrig.GetX() < tiles.GetXSize() && ptOrig.GetY() < tiles.GetYSize() ) // CRAP
			fHeight += GetFHeight( tiles[ptOrig.GetY()][ptOrig.GetX()].nHeight );
		return CVec3( GetCPNoHeight( x, y ), fHeight );
		//return CVec3( ptOrigin.x + ptXDir.x * x - ptXDir.y * y, ptOrigin.y + ptXDir.y * x + ptXDir.x * y, fHeight );
	}
	float GetDirection( const SPathPlace &p ) const
	{
	//	float fAngle = atan2( ptXDir.y, ptXDir.x ) + p.GetDirection() * FP_PI4;
	//	return NormalizeAngleInRadian( fAngle );
		return pGroup->GetDirection(p);
	}
	int GetFloor() const { return nFloor; }
	CNodesLayer(): bHappySave( false ) {}
	//void ForceRecalc( const SSphere &s );
	void AddNearPoints( const SSphere &s, vector<SPathPlace> *pRes, bool bTakeAll );
	void BuildLayer( int nXSize, int nYSize, const CVec2 &_ptOrigin, const CVec2 &_ptXDir, 
		int nFloor, int nLayer, CPathNetwork *_pNet, CLayersGroup* pGroup = 0 );
	bool IsValidDestination( const SPathPlace &p );
	//bool GetLadder(int *nLadder, int *_nLayer, const SPathPlace &pt);
	
	friend class CPathNetwork;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CColouredWaysCalcer;
class IAIJobManager;
typedef vector<SPathPlace> CPointsContainer;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathNetwork: public IPathNetwork
{
	OBJECT_BASIC_METHODS(CPathNetwork);
	struct SLockInfo
	{
		ZDATA
		bool bLocked;
		vector<SPathPlace> places;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bLocked); f.Add(3,&places); return 0; }
		SLockInfo() {}
		SLockInfo( const vector<SPathPlace> &_p ): places(_p), bLocked(true) {}
	};
	typedef unordered_map<CPtr<CObjectBase>, SLockInfo, SPtrHash> CLocksHash;
	struct SDynLockInfo
	{
		ZDATA
		vector<SPathPlace> points;
		bool bLocked;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&points); f.Add(3,&bLocked); return 0; }
	};
public:
	enum EFixed
	{
		F_VISIBLE_TRAP = 1,
		F_LOCKED_DOOR = 2,
	};
	struct SFlipper
	{
		ZDATA
		int nFlipper;
		unordered_map<SPathPlace, CNodesLayer::STile, SPathPlaceHash> locksOpen;
		unordered_map<SPathPlace, CNodesLayer::STile, SPathPlaceHash> locksClosed;
		bool bOpen;
		int nFixedFlags;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nFlipper); f.Add(3,&locksOpen); f.Add(4,&locksClosed); f.Add(5,&bOpen); f.Add(6,&nFixedFlags); return 0; }
		SFlipper() {}
	};
	typedef unordered_map<CPtr<CObjectBase>, SDynLockInfo, SPtrHash> CDynLocksHash;
	typedef unordered_map<CPtr<CObjectBase>, int, SPtrHash> CFlippersHash;
private:
	ZDATA
	CPtr<IAIMap> pMap;
	vector< CObj<CNodesLayer> > layers;
	CLocksHash lockedPlaces;
	CDynLocksHash dynLockedPlaces;
	bool bColouringConstructed;
	CPointsContainer oldAccountPoints;  // theese are account locked points used last time in the pathfinder
	CObj<CColouredWaysCalcer> pWaysCalcer;
	vector<SFlipper> flippers;
	CFlippersHash flippersHash;
	vector< CPtr<CLayersGroup> > groups;
	bool bFreeze;
	vector<SPathPlace> mineTempLocks;
	CPtr<CObjectBase> pJobManager;
	bool bDoNotFlipAnything;   // retail tag 14: flipper freeze -- toggled by NAI::FindPath's door-free re-route (@0x8bd80), read by IsLocked (@0x45520)
	bool bHappySave;           // retail tag 15: 'small save' -- strip recolour caches (SetSmallSaveSize @0x3db60; NO retail caller -- vtbl-only dead code, see W3 report)
	bool bUpdated;             // retail tag 17: set by CLayersGroup::RecalcSquare tail (@0x41010), read-and-cleared by CheckUpdated (@0x4c9a0)
	// retail @0x46d40: tags 2..13 as before + 14 bDoNotFlipAnything, 15 bHappySave, 16 pJobManager,
	// 17 bUpdated; a happy save writes FALSE for bColouringConstructed at tag 6 (caches stripped).
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&pMap); f.Add(3,&layers); f.Add(4,&lockedPlaces); f.Add(5,&dynLockedPlaces);
		if ( !f.IsReading() && bHappySave )
		{
			bool bFalse = false;
			f.Add(6,&bFalse);
		}
		else
			f.Add(6,&bColouringConstructed);
		f.Add(7,&oldAccountPoints); f.Add(8,&pWaysCalcer); f.Add(9,&flippers); f.Add(10,&flippersHash);
		f.Add(11,&bFreeze); f.Add(12,&groups); f.Add(13,&mineTempLocks);
		f.Add(14,&bDoNotFlipAnything); f.Add(15,&bHappySave); f.Add(16,&pJobManager); f.Add(17,&bUpdated);
		return 0;
	}
	
	void AddLink( const SPathPlace &p, CNodesLayer *pLayer, vector<SShowLink> *pLinks, 
		int nDir, const CTPoint<int> &shift, char nMoveFlags, SShowLink::EType type );
	void GetLockAreaInternal( vector<SPathPlace> *pRes, const SPathPlace &p ) const;
	void GetLockAreaInternal( vector<SPathPlace> *pRes, const SPathPlace &p, int nDX, int nDY ) const;
	void LockUnlockPoint( const SPathPlace &p, bool bLock );
	void LockUnlockDynamic( const SPathPlace &p, bool bLock, bool bBigUnit );
	void Lock( CObjectBase *pUnit, const vector<SPathPlace> &places );
	void DisableLocks( CObjectBase *p );
	void FinishGridConstruction();
	void FinishLaddersConstruction();
	void CreateLaddersInternal( CLayersGroup *pGroup );
	bool IsNotOnDoor( const SPathPlace &p ) const;

public:
	CPathNetwork( IAIMap *_pMap = 0, IAIJobManager *pManager = 0 );
	void SetSmallSaveSize( bool bSmall );   // retail @0x3db60 -- flag net+layers for a cache-stripped save
	// retail @0x4c9a0 (IAIMap vtbl slot): read-and-clear "network changed since last query".
	// Retail consumer = CWorld::Segment @0x36bce0 -> GridInfoUpdated @0x375ca0 (TBS_GRID_INFO_UPDATED).
	virtual bool CheckUpdated() { bool bWas = bUpdated; bUpdated = false; return bWas; }
	// dev access shim for the retail direct member writes (retail NAI::FindPath @0x8bd80 toggles
	// bDoNotFlipAnything around the door-free re-route; the member is private here).
	void SetDoNotFlipAnything( bool b ) { bDoNotFlipAnything = b; }

	const vector<CObj<CNodesLayer> >& GetLayers() const { return layers; }
	
	IAIMap* GetAIMap() const { return pMap; }
	CObjectBase* GetAIJobManager() const { return pJobManager; }

	int CreateLayer( int nXSize, int nYSize, const CVec2 &ptOrigin, const CVec2 &ptXDir, int nFloor, CLayersGroup* pGroup ); 
	virtual int CreateLayersGroup( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFirstFloor );
	virtual int CreateLayer( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFloor, CLayersGroup* pGroup = 0 );
	virtual EDirection GetDir( const SPathPlace &src, const SPathPlace &dst );
	virtual EDirection GetClosestDir( const SPathPlace &src, const SPathPlace &dst );
	virtual EDirection GetClosestDir( int nLayer, float fAngle );
	virtual bool SetOnLayer( SPosition *pRes, int nLayer, const CVec3 &pos );
	virtual void SetOnLayer( SUnitPosition *pRes, int nLayer, const CVec3 &pos );
	virtual void SetOnLayer( SObjectPosition *pRes, int nLayer, const CVec3 &pos );
	virtual bool SetOnFloor( SPosition *pRes, int nFloor, const CVec3 &pos );
	virtual int GetNumLayers() const { return layers.size(); }
	virtual void GetPassability( CArray2D<bool> *pRes, int nLayer );
	virtual void GetNetworkFragment( const SPosition &pos, bool bOneColorOnly, vector<SShowPoint> *pKnots, vector<SShowLink> *pLinks );
	virtual int GetFloor( int nLayer ) const;
	virtual void CreateAlternativeGrids( const SAlternativeGridInfo &root );
	virtual void Lock( CObjectBase *pUnit, const SPathPlace &p );
	virtual void LockMovingObject( CObjectBase *pUnit, const SPathPlace &p1, const SPathPlace &p2 );
	virtual void Unlock( CObjectBase *pUnit );
	virtual void LockSelected( const list<CObjectBase*> &selected );
	virtual void UnlockSelected();
	virtual bool IsLocked( const SPathPlace &p, bool bIgnoreBlockedDoors = false ) const;
	virtual bool IsBigLockerLocked( const SPathPlace &p, EBigLockerType type ) const;
	virtual CObjectBase* GetWhoLocksThisPlace( const SPathPlace &p ) const;
	virtual void ClearDynamicLocks( CObjectBase *pUnit );
	virtual void RestoreDynamicLocks( CObjectBase *pUnit );
	virtual void ChangeDynamicLocks( CObjectBase *pUnit, const vector<SPathPlace> &points );
	virtual bool IsValidDestination( const SPathPlace &p );
	virtual bool IsPassable( const SPathPlace &p );
	virtual EPassable GetPassability( const SPathPlace &p );   // @0x42e30 granular sibling; IsPassable == (GetPassability==AIP_YES)
	bool IsBlockedByFlipper( const SPathPlace	&from, const SPathPlace	&to, CPtr<CObjectBase> *ppFlipper,
		bool *bIsNowOpen, bool *bBlocksInOpenState, bool *bBlocksInClosedState );
	virtual bool IsNativePassable( const SPathPlace &p );
	virtual void GetNearPlaces( const SSphere &s, vector<SPathPlace> *pRes, bool bTakeAll = false );
	virtual void CreateLadder( const CVec2 &ptPos, const CVec2 &ptUpperPos, int nHeight, int nFloor );   // retail @0x40290
	virtual bool UpdateColouring( const vector<SPathPlace> &lockers );
	virtual void FlipperOpenClose( CObjectBase* flipper, bool bOpen );
	virtual void LockUnlockFlipper( CObjectBase* flipper, bool bLock );
	virtual void GetLockArea( vector<SPathPlace> *pRes, const SPathPlace &p, bool bBigUnit ) const;

	CVec3 GetCP( const SPathPlace &src ) const;
	CVec2 GetCPNoHeight( const SPathPlace &src ) const;
	float GetDirection( const SPathPlace &src ) const;
	int GetFloor( const SPathPlace &src ) const;

	SFlipper *GetFlipper( int i ) { return &flippers[i]; }
	const SFlipper *GetFlipper( int i ) const { return &flippers[i]; }
	SFlipper *GetFlipper( const CObjectBase *pSrc );

	bool HasLowerSame( const SPathPlace &p );
	bool IsGridReady() { return bColouringConstructed; }
	void AddDirectTransitions( const CNodesLayer::CTransitionsHash &newtrans );
	CNodesLayer* GetLayer( int nLayer ) const { ASSERT( nLayer < layers.size() && nLayer >= 0 ); return layers[nLayer]; }
	CMapColourer* GetColourer( int nLayer ) const { return layers[nLayer]->pColourer; }
	CColouredWaysCalcer* GetWaysCalcer() { return pWaysCalcer; };
	void PrintConsoleInfo( SPathPlace p );
	void PrintLayerInfo( int nLayer );
	virtual void FormationMoveTo( vector<SPosition> *pPlaces, const SPosition &to );
	virtual void Freeze( bool bFreeze );
	virtual SPathPlace GetDeployPlace( const SPathPlace &start, int nDisplacement );
	virtual bool HasPassCalcerJobs() const;
	friend class CNodesLayer;
	friend class CLayersGroup;   // RecalcSquare flags the network updated (retail @0x41010 tail)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
