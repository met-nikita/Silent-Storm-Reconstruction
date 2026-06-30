#ifndef __AIPOSITION_H_
#define __AIPOSITION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\2Darray.h"
namespace NAI
{
class CPathNetwork;
class CLayersGroup;
class IPathNetwork;
class IAIMap;
struct SHeightCalcInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
// place on NodesNetwork
enum ECheckMove
{
	CM_LAY = 0,
	CM_CROUCH,
	CM_STAND,
	CM_INACTIVE,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETransitionType
{
	TT_NO_WAY,
	TT_SAME,
	TT_MOVE,
	TT_MOVE_DIAGONAL,
	TT_TURN,
	TT_CLIMB_1,
	TT_CLIMB_2,
	TT_CLIMB_3,
	TT_CLIMB_4,
	TT_JUMP,
	TT_POSE,
	TT_INTERGRID_SAME,
	TT_INTERGRID,
	TT_LADDER_UP,
	TT_LADDER_DOWN,
	TT_LADDER_MOVE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMoveType
{
	MT_MOVE_STAND,
	MT_MOVE_STAND_DIAG,
	MT_MOVE_CROUCH,
	MT_MOVE_CROUCH_DIAG,
	MT_MOVE_CRAWL,
	MT_MOVE_CRAWL_DIAG,
	MT_TURN,
	MT_CLIMB_1,
	MT_CLIMB_2,
	MT_CLIMB_3,
	MT_CLIMB_4,
	MT_JUMP,
	MT_POSE_WALK_CRAWL,
	MT_POSE_WALK_CROUCH,
	MT_POSE_CROUCH_CRAWL,
	MT_LADDER_UP,
	MT_LADDER_DOWN,
	MT_LADDER_MOVE,
	MT_ZERO,
	N_MOVE_TYPES
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFindPathParams
{
	PF_DEFAULT = 0,
	PF_USE_DIR = 1,
	PF_USE_POSE = 2,
	PF_USE_POSEDIR = 3,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 int nMoveShift[][2];
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPathPlace
{
private:
	union
	{
		struct  
		{
			union
			{
				struct 
				{
					unsigned short nX:8;
					unsigned short nY:8;
				};
				unsigned short nPoint:16;
			};
			unsigned short nIntegral:1;
			unsigned short nLayer:8;
			unsigned short nFinal:1;
			unsigned short nMoving:1;
			unsigned short nDirection:3;
			unsigned short nPose:2;  // CM_*
		};
		int nData;
	};
public:
	SPathPlace() { nData = -1; nFinal = 0; }
	SPathPlace( int _nData ): nData(_nData) {}
	SPathPlace( unsigned _nX, unsigned _nY, unsigned _nLayer )
		: nX(_nX), nY(_nY), nLayer(_nLayer), nDirection(0), nPose(0), 
		nMoving(0), nFinal(0), nIntegral(1) {}
	SPathPlace( unsigned _nX, unsigned _nY, unsigned _nLayer, unsigned _nDirection, 
		unsigned _nPose, unsigned _nMoving )
		: nX(_nX), nY(_nY), nLayer(_nLayer), nDirection(_nDirection), nPose(_nPose), 
		nMoving(_nMoving), nFinal(0), nIntegral(1) {}
	bool IsFinal() const { return nFinal; }
	int GetData() const { return nData; }
	unsigned short GetLayer() const { return nLayer; }
	unsigned short GetDirection() const { return nDirection; }
	unsigned short IsMoving() const { return nMoving; }
	unsigned short GetPose() const { return nPose; }
	bool IsIntegral() const { return nIntegral; }
	unsigned short GetX() const { return nX; }
	unsigned short GetY() const { return nY; }
	unsigned short GetLadderStep() const { ASSERT( !nIntegral ); return nY; }
	void SetXY( unsigned int _nX, unsigned int _nY ) { nX = _nX; nY = _nY; }
	void SetY( unsigned int _nY ) { nY = _nY; }
	void SetDirection( unsigned short _n ) { nDirection = _n; }
	void SetPose( unsigned short _n ) { nPose = _n; } // CM_*
	void SetOnLayer( int _nLayer, int _nX, int _nY, int _nIntegral = 1 ) { nX = _nX; nY = _nY; nIntegral = _nIntegral; nLayer = _nLayer; nPose = 0; nFinal = 0; }
	void SetFinal( unsigned n ) { nFinal = n; }
	void SetMoving( unsigned n ) { nMoving = n; }
	bool operator==( const SPathPlace &a ) const { return nData == a.nData; }
	SPathPlace& operator=( const SPathPlace &a ) { nData = a.nData; return *this; }
};
struct SPathPlaceHash
{
	int operator()( const SPathPlace &a ) const { return a.GetData(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef CTPoint<int> SPoint;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPosition
{
	SPathPlace p;
	CPtr<CPathNetwork> pNet;
	
	SPosition() {}
	SPosition( const SPathPlace &_p, IPathNetwork *_pNet );
	void SetNetwork( IPathNetwork *_pNet );
	IPathNetwork* GetNetwork() const { return (IPathNetwork*)pNet.GetPtr(); }
	bool IsValid() const;
	CVec2 GetCPNoHeight() const;
	CVec3 GetCP() const;
	float GetDirection() const;
	int GetFloor() const;
	int GetLayer() const { return p.GetLayer(); }
	bool operator==( const SPosition &a ) const { return p == a.p && pNet == a.pNet; }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectPosition
{
	SPosition pos;
	int nFloor;

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPose	// �� ������ �������! ��� ��������� �� ���������� AP!
{
	CRAWL = 0,	// �������
	CROUCH,		// ������
	WALK,		// ����
	RUN			// �����
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EDirection
{
	RIGHT= 0, UPRIGHT, UP, UPLEFT, LEFT, DOWNLEFT, DOWN, DOWNRIGHT, NONE
};
enum EHitLocation
{
	HL_ANY = -1,
	HL_BODY = 0,
	HL_HEAD,
	HL_RHAND,
	HL_LHAND,
	HL_RLEG,
	HL_LLEG,
	N_HL
};
enum ETileHitLocation
{
	THL_LOWER = 0,
	THL_MIDDLE,
	THL_UPPER
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitPosition
{
	SPosition pos;
	bool bRun;

	bool IsValid() const;
	CVec2 GetCPNoHeight() const { return pos.GetCPNoHeight(); }
	CVec3 GetCP() const { return pos.GetCP(); }
	float GetDirection() const { return pos.GetDirection(); }
	EDirection GetDir() const { return (EDirection)( pos.p.GetDirection() ); }
	void SetPose( EPose pose );
	EPose GetPose() const;
	float GetHeight() const;
	float GetHLHeight( EHitLocation eHL ) const;
	CVec3 GetCenter() const;
	CVec3 GetEyePosition() const;
	bool operator==( const SUnitPosition &a ) const { return pos == a.pos && bRun == a.bRun; }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EBlowHeight // Close combat
{
	BH_TOP,
	BH_MIDDLE,
	BH_BOTTOM,
};
EBlowHeight GetBlowHeight( const SUnitPosition &attackerPos, const CVec3 &ptTarget );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMove
{
	union
	{
		struct 
		{
			SPathPlace dest;
			EMoveType type;
		};
		struct 
		{
			SPathPlace first;
			EMoveType second;
		};
	};
	SMove() {};
	SMove &SMove::operator=(const SMove& src)
	{
		this->dest = src.dest;
		this->type = src.type;
		this->first = src.first;
		this->second = src.second;
		return *this;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SShowLink
{
	enum EType
	{
		ANY_MOVE,
		DIRECT,
		STAND_MOVE,
		CROUCH_MOVE,
		CRAWL_MOVE,
		HEIGHT_CHANGE,
		NEIGHBOUR_ZONE_STAND_ONLY,
		NEIGHBOUR_ZONE_ANY_MOVE,
		LAY_POSE_SHOW
	};
	CVec3 start, finish;
	EType t;
	
	SShowLink() {}
	SShowLink( const CVec3 &a, const CVec3 &b, EType _t ): start(a), finish(b), t(_t) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SShowPoint
{
	enum EFlags
	{
		CAN_STAND     = 1,
		CAN_CROUCH		= 2,
		CAN_LAY				=	4,
		EVERY_POSE = 15,
		BOW_LEGGED = 8,
		COLOR_CENTER = 16,
		LOCKED = 0x80
	};
	CVec3 pos;
	int nFlags;

	SShowPoint() {}
	SShowPoint( const CVec3 &_pos, int _nFlags ): pos(_pos), nFlags(_nFlags) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EBigLockerType
{
	BL_PANZERKLEINE,
	BL_CAR,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAlternativeGridInfo
{
	int nLayersGroup;
	vector<SAlternativeGridInfo> children;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPathNetwork: public CObjectBase
{
public:
	virtual int CreateLayersGroup( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFirstFloor ) = 0;
	virtual int CreateLayer( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFloor, CLayersGroup* _pGroup = 0 ) = 0;
	virtual EDirection GetDir( const SPathPlace &src, const SPathPlace &dst ) = 0;
	virtual EDirection GetClosestDir( const SPathPlace &src, const SPathPlace &dst ) = 0;	
	virtual EDirection GetClosestDir( int nLayer, float fAngle ) = 0;
	virtual bool SetOnLayer( SPosition *pRes, int nLayer, const CVec3 &pos ) = 0;
	virtual void SetOnLayer( SUnitPosition *pRes, int nLayer, const CVec3 &pos ) = 0;
	virtual void SetOnLayer( SObjectPosition *pRes, int nLayer, const CVec3 &pos ) = 0;
	virtual bool SetOnFloor( SPosition *pRes, int nFloor, const CVec3 &pos ) = 0;
	//virtual void GetMoves( const SPathPlace &src, bool bCheckSuicide, bool bMoveOnly, vector<SMove> *pRes ) = 0;
	virtual int GetNumLayers() const = 0;
	virtual void GetPassability( CArray2D<bool> *pRes, int nLayer ) = 0;
	virtual void GetNetworkFragment( const SPosition &pos, bool bOneColorOnly, vector<SShowPoint> *pKnots, vector<SShowLink> *pLinks ) = 0;
	virtual int GetFloor( int nLayer ) const = 0;
	virtual void CreateAlternativeGrids( const SAlternativeGridInfo &root ) = 0;
	virtual void Lock( CObjectBase *pUnit, const SPathPlace &p ) = 0;
	virtual void LockMovingObject( CObjectBase *pUnit, const SPathPlace &p1, const SPathPlace &p2 ) = 0;
	virtual void Unlock( CObjectBase *pUnit ) = 0;
	virtual void LockSelected( const list<CObjectBase*> &selected ) = 0;
	virtual void UnlockSelected() = 0;
	virtual bool IsLocked( const SPathPlace &p, bool bIgnoreBlockedDoors = false ) const = 0;
	virtual bool IsBigLockerLocked( const SPathPlace &p, EBigLockerType type ) const = 0;
	virtual CObjectBase* GetWhoLocksThisPlace( const SPathPlace &p ) const = 0;
	virtual void ClearDynamicLocks( CObjectBase *pUnit ) = 0;
	virtual void RestoreDynamicLocks( CObjectBase *pUnit ) = 0;
	virtual void ChangeDynamicLocks( CObjectBase *pUnit, const vector<SPathPlace> &points ) = 0;
	virtual bool IsValidDestination( const SPathPlace &p ) = 0;
	virtual bool IsPassable( const SPathPlace &p ) = 0;
	virtual bool IsNativePassable( const SPathPlace &p ) = 0;
	virtual void GetNearPlaces( const SSphere &s, vector<SPathPlace> *pRes, bool bTakeAll = false ) = 0;
	//virtual void ForceLayersRecalc( const SSphere &s ) = 0;
	//virtual ETransitionType GetTransitionType( const SPathPlace &from, const SPathPlace &to ) = 0;
	virtual void CreateLadder( int nX, int nY, int nHeight, int nRotation, int nLayersGroup, int nFloor ) = 0;
	virtual bool UpdateColouring( const vector<SPathPlace> &newLockers ) = 0;
	virtual void FormationMoveTo( vector<SPosition> *pPlaces, const SPosition &to ) = 0;	
	virtual void FlipperOpenClose( CObjectBase* flipper, bool bOpen ) = 0;
	virtual void LockUnlockFlipper( CObjectBase* flipper, bool bLock ) = 0;
	virtual void Freeze( bool bFreeze ) = 0;
	virtual SPathPlace GetDeployPlace( const SPathPlace &start, int nDisplacement ) = 0;
	virtual void GetLockArea( vector<SPathPlace> *pRes, const SPathPlace &p, bool bBigUnit ) const = 0;
	// true while any layer group still has a pending recolour/pass-calc job (PassCalcerIsActive /
	// WaitForPassCalc). Retail CPathNetwork::HasPassCalcerJobs @0x3e4a0.
	virtual bool HasPassCalcerJobs() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIJobManager;
IPathNetwork* CreateNodesNetwork( IAIMap *pMap, IAIJobManager *pJobManager );
// Snap *pOut onto layer 0 near *pSrc's world position, then repurpose its nLayer field to hold the
// altitude above that ground cell (in F_3D_STEP units, clamped 0..255) and flag it as a 3D/fly
// position (nFinal). Used by UnitFlyToWaypoint -> CCmdFly. Retail NAI::MakeFlyPos @0x3d0e0/@0x3d180.
bool MakeFlyPos( SPosition *pSrc, SPosition *pOut );
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiPositionDebug -- locker-validity probes for CPathNetwork::DebugCheck (retail @0x917d0 / @0x91810);
// defined in aiPositionDebug.cpp. Caller absent in dev -> reconstructed but unwired (parity surface).
bool IsLockerUnit( CObjectBase *pUnit );                 // live CUnitServer whose IsLocker() holds
bool IsUnitNear( CObjectBase *pUnit, const CVec3 &pt );  // center within 2 (4 big-locker) grid steps of pt
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif