#ifndef __wOSBase_H_
#define __wOSBase_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "wInterface.h"
#include "wInterfaceVisitors.h"
#include "RPGAttackMech.h"
#include "wDynObject.h"
namespace NAnimation
{
	class CSkeletonAnimator;
	class CSkeletonState;
	class CAnimation;
}
namespace NDb
{
	class CObject;
	class CDebrisMaterial;
	class CContainerModel;
	class CRPGGrenade;
	class CModel;
}
namespace NRPG
{
	class IObject;
}
namespace NAI
{
	class IStabilityTrackers;
}
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectPlace
{
	CVec3 ptPos, ptScale;
	float fAngle;
	int nFloor;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorld;
enum ETimeOfDay : int;   // defined in wMain.h (retail NDb::ETimeOfDay; TOD_ANYTIME=0)
class CObjectServerBase: public IObject, public NRPG::IAttackable, public IVisObj
{
protected:
	ZDATA
	CObj<NRPG::IObject> pRPG;
	CPtr<NDb::CObject> pDbObject;
	// dev-internal runtime cache of pRPG's stage (refreshed at the top of every Visit); retail has NO such
	// member (PDB: 12 members, size 124) and reads the stage through pRPG each time (GetDestroyStage @0x383c10),
	// so it is OFF the wire -- retail operator& @0x375520 skips tag 4 (W5).
	int nDestroyStage;
	bool bLightMap;
	CSyncSrcBind<IVisObj> bindGlobal;
	SObjectPlace position;
	CPtr<CWorld> pWorld;
	STime tStageChange;
	STime tLastSound;
	vector<int> vCreateFlags;
	bool bBorder;
	// retail @offset 100 (CDBPtr<NDb::CRPGGrenade>) -- the explosion currently ARMED on this object
	// (gas-tank/barrel self-detonation). Armed by AttachExplosion (ProcessAttack path B), fired+cleared by
	// path A; serialized as chunk 13 in operator& below, matching retail operator& @0x375520, so an object
	// saved mid-arm restores its pending explosion across load. CDBPtr default-ctors null.
	CDBPtr<NDb::CRPGGrenade> pAttachedGrenade;
	// retail @offset 104 (tag 14, NDb::ETimeOfDay): the light's time-of-day activity window, seeded from
	// NDb::CFinalElement::eTimeOfDay by the map builder (retail ctor @0x3862e0); gates AddLights @0x383d50.
	ETimeOfDay eLightActivity;
public:
	// retail operator& @0x375520: {2,3,5..13,14} -- tag 4 is a permanent gap (no nDestroyStage on the wire),
	// tag 14 = eLightActivity (4-byte DataChunk).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pRPG); f.Add(3,&pDbObject); f.Add(5,&bLightMap); f.Add(6,&bindGlobal); f.Add(7,&position); f.Add(8,&pWorld); f.Add(9,&tStageChange); f.Add(10,&tLastSound); f.Add(11,&vCreateFlags); f.Add(12,&bBorder); f.Add(13,&pAttachedGrenade); f.Add(14,&eLightActivity); return 0; }
protected:
	void Kill( const CVec3 &ptDir );
	bool CreateTransform( SFBTransform *pRes );
	NDb::CDebrisMaterial* GetDebrisMaterial() const;
	void AddLights( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv );
	void AddEffects( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv );
	void AddEffects( IAIVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv );
	void AddObject( IRenderVisitor *p, NDb::CObject *pO, const SFBTransform &rv );
	void AddObject( IAIVisitor *p, NDb::CObject *pO, const SFBTransform &rv );
	void AddObject( ISoundVisitor *p, NDb::CObject *pO, const SFBTransform &rv );
	CWorld* GetWorld() const { return pWorld; }
	const vector<int>& GetCreateFlags() const { return vCreateFlags; }
	void PrecacheEffects( IAIVisitor *p, NDb::CObject *pO );
	void AddObjectHull( IAIVisitor *pVisitor, 
		NDb::CAIGeometry *pGeometry, const SFBTransform &rv, NDb::CRPGArmor *pArmor, int nFloor );
	int GetDecalID();
public:
	// old dev saves carry neither tag 4 nor tag 14 -- default both caches to a defined state (0 = TOD_ANYTIME)
	CObjectServerBase(): nDestroyStage( 0 ), eLightActivity( ETimeOfDay(0) ) {}
	// retail ctor @0x3862e0: eTimeOfDay (NDb::ETimeOfDay) sits between vCreateFlags and bBorder
	CObjectServerBase( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay, bool _bBorder = false );
	
	bool CheckStability();
	// retail @0x383fe0 / @0x384060 -- see wOSBase.cpp
	bool GetCheckStabilityParams( NDb::CModel **ppModel, SHMatrix *pMatrix );
	void RegisterForStability( NAI::IStabilityTrackers *pTrackers );

	// NRPG::IAttackable
	virtual int ProcessAttack( NWorld::IWorld *_pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
		const CVec3 &vDir, NDb::CRPGArmor *pArmor );
	// IObject
	virtual bool IsTargetable() const;
	// IVisObj
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	virtual void Visit( ISoundVisitor *p );

	virtual bool NeedSegment() const;
	virtual bool Segment();

	virtual void SetPosition( const SObjectPlace &pos );
	const SObjectPlace& GetPosition() const { return position; }

	// for debug purposes:
	int GetDBObjectID() const;
	virtual void SetDestroyStage( int nStage );
	virtual int GetHP();             // luaObjectGetHP @0x2e9260 (delegates to the RPG object's nVP)
	int GetDestroyStage();           // luaObjectGetDestroyStage @0x2e8ec0 (delegates to the RPG object)
	int GetTimeSinceLastStageChange(); // wOSBase.obj @0x383c00 -- ticks since the last destroy-stage change
	void UpdateLight();              // retail @0x384ea0 -- re-sync the object's vis binding (re-light it for a new TOD)
	void BeAddedToVisitiors( bool bAdd ); // retail [sic] -- (un)bind the global vis sync (object-pocket support)
	void AttachExplosion( NDb::CRPGGrenade *pGrenade ); // retail @0x372170 (IObject vtbl slot +0x10) -- arm-only setter of pAttachedGrenade
	void MakeDestroySound();         // retail @0x384100 -- current stage's destroy sound, throttled to >99 ticks since the last stage change
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IGetApproaches
{
public:
	virtual void GetApproaches( vector<NAI::SPathPlace> *pRes, NAI::IPathNetwork *pNet ) const = 0;
	virtual void GetApproachPts( vector<CVec3> *pRes ) const = 0; // now used only in debug
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CActionCounter;
class CAnimObjectServerBase: public CObjectServerBase, public IGetApproaches, public IDynamicObject
{
protected:
	ZDATA_(CObjectServerBase)
	CDBPtr<NDb::CSkeleton> pSkeleton;
	CObj<CFuncBase<STime> > pTime;
	CObj<NAnimation::CSkeletonAnimator> pAnimator;
	CObj<NAnimation::CSkeletonState> pState;
	CObj<CActionCounter> pAction;
	STime tEnd;
	vector<CObj<CObjectBase> > destroyAnimations;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CObjectServerBase*)this); f.Add(2,&pSkeleton); f.Add(3,&pTime); f.Add(4,&pAnimator); f.Add(5,&pState); f.Add(6,&pAction); f.Add(7,&tEnd); f.Add(8,&destroyAnimations); return 0; }
protected:
	void IdleOn();
	void IdleOff();
	void PlayAnimation( bool bInstantly = false );
	void PlayCustomAnimation( NAnimation::CAnimation *pAnimation, bool bInstantly = false );
	void AddEffects( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv );
	void PrecacheAnimations();
	void PrecacheAIGeom( IAIVisitor *p );
public:
	CAnimObjectServerBase() {}
	// retail ctor @0x386510: eTimeOfDay appended after vCreateFlags
	CAnimObjectServerBase( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );

	// IGetApproaches
	virtual void GetApproaches( vector<NAI::SPathPlace> *pRes, NAI::IPathNetwork *pNet ) const;
	virtual void GetApproachPts( vector<CVec3> *pRes ) const;
	// IVisObj
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	// IDynamicObject
	virtual bool Segment();
	//
	virtual void SetPosition( const SObjectPlace &pos );
	void PlayDBAnimation( int nDBAnimationID );
	// for script
	bool IsPerformingAction();
	void CancelAction();
	//
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMask( NDb::CAIGeometry *pAIGeometry, NDb::CRPGArmor *pArmor );
}
namespace NAI { struct SSourceInfo; }
namespace NWorld
{
// wCheckGlassGrenade.obj @0x347bd0 -- grenade-vs-breakable-glass collider gate (drives breakable-glass
// destroy stages through CObjectServerBase). phCollider's friction collider is its release call site.
bool CheckItemsBreakGlass( const NAI::SSourceInfo *pSrc );
}
#endif
