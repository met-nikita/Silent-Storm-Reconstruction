#ifndef __GANIMATION_H_
#define __GANIMATION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GAnimBase.h"
#include "GAnimPath.h"
#include "GAnimTerrain.h"
#include "GAnimFormat.h"
#include "../DBFormat/DataAnimation.h"
#include "../DBFormat/DataGeometry.h"
#include "../DBFormat/DataSound.h"
#include "../DBFormat/DataRPG.h"
#include "aiMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CCInt;
}
namespace NWorld { class CUnitServer; }   // fwd for AddDynamics' pServer (-> CParticleSkeleton::pUnit)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETransitType
{
	LINEAR,
	SINUS,
	PARABOLA,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionLinear : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionLinear);
	STime tStart;
	STime tEnd;
	float fStartValue;
	float fEndValue;
public:
	CAFunctionLinear() : tStart(0), tEnd(0), fStartValue(0), fEndValue(0) {}
	CAFunctionLinear( STime t1, STime t2, float fSVal = 0, float fEVal = 1.f ) :
		tStart( t1 ), tEnd( t2 ), fStartValue(fSVal), fEndValue(fEVal) {}
	virtual float GetValue( STime t );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionConstant : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionConstant);
	float fValue;
public:
	CAFunctionConstant( float _fValue = 0 ) : fValue( _fValue ) {}
	virtual float GetValue( STime t ) { return fValue; }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionSinus : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionSinus);
	STime tStart;
	STime tEnd;
public:
	CAFunctionSinus() : tStart(0), tEnd(0) {}
	CAFunctionSinus( STime t1, STime t2 ) : tStart( t1 ), tEnd( t2 ) {}
	virtual float GetValue( STime t );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionParabola : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionParabola);
	STime tStart;
	STime tEnd;
public:
	CAFunctionParabola() : tStart(0), tEnd(0) {}
	CAFunctionParabola( STime t1, STime t2 ) : tStart( t1 ), tEnd( t2 ) {}
	virtual float GetValue( STime t );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionCos : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionCos);
	STime tStart;
	STime tPeriod;

public:
	CAFunctionCos() : tStart(0), tPeriod(1000) {}
	CAFunctionCos( STime t1, STime t2 ) : tStart( t1 ), tPeriod( t2 ) {}
	virtual float GetValue( STime t ) { return cos( FP_2PI * (t-tStart) / tPeriod ) * 0.5f + 0.5f; }
	int operator&( CStructureSaver &f ) { f.Add( 1, &tStart ); f.Add( 2, &tPeriod ); return 0;	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimation : public CAnimator
{
	OBJECT_BASIC_METHODS(CAnimation);
	ZDATA_(CAnimator)
	vector<int> nBoneIndices; // correspondence: skeleton bone index -> animation bone index

	CPtr< CAnimator > pPos;
	CDBPtr< NDb::CAnimation > pA;
	CDGPtr< CPtrFuncBase<CFileAnimationInfo> > pAnim; // file animation
	CPtr< CAFunction > pFunc; // time warping function
	STime tStart;
	bool bCycle;
	STime tLength;

	bool bMoveCycle;
	CVec3 start;
	CVec3 end;
	CVec3 move;
	float fVelocity;
	float fDistance;
	CVec3 shift;

	bool bRotateCycle;
	CQuat rotate;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&nBoneIndices); f.Add(3,&pPos); f.Add(4,&pA); f.Add(5,&pAnim); f.Add(6,&pFunc); f.Add(7,&tStart); f.Add(8,&bCycle); f.Add(9,&tLength); f.Add(10,&bMoveCycle); f.Add(11,&start); f.Add(12,&end); f.Add(13,&move); f.Add(14,&fVelocity); f.Add(15,&fDistance); f.Add(16,&shift); f.Add(17,&bRotateCycle); f.Add(18,&rotate); return 0; }
public:
	CAnimation() {}
	CAnimation( NDb::CAnimation *pA, CPtrFuncBase<CFileSkeletonInfo> *pSkeleton, STime _tStart );

	// positions
	void SetStand( STime t, const CVec2 &pos, float fAngle );
	void SetStand( STime t, const CVec3 &pos, float fAngle );
	void SetTrajectory( STime t, float fVelocity, float fTStart, CPathInterpolator *pPath );
	void SetMove( STime t, const CVec3 &pos, float fAStart, const CVec3 &vel, float fAVel );
	void SetInventory( STime t, const CVec3 &pos, const CVec3 &angle );
	
	void SetInterval( STime _tStart, STime _tEnd );
	void SetTimeFunction( CAFunction *_pFunc ) { pFunc = _pFunc; }
	void SetCycle( bool _bCycle ) { bCycle = _bCycle; }
	void SetMoveCycle( bool _bMoveCycle ) { bMoveCycle = _bMoveCycle; }
	void SetRotateCycle( bool _bRotateCycle ) { bRotateCycle = _bRotateCycle; }
	void SetShift( CVec3 &_shift ) { shift = _shift; }

	const STime GetOriginalTime();
	const STime GetTime() const { return tLength; }
	const CVec3 GetStart() const { return start; }
	const CVec3 GetEnd() const { return end; }
	const CVec3 GetMove() const { return move; }
	const float GetDistance() const { return fDistance; }
	const float GetVelocity() const { return fVelocity; }
	const STime GetTimeLabel1();
	const STime GetStepLabel1();
	const STime GetStepLabel2();
	const float GetAngle() const;

	virtual void GetBoneFrame( STime t, SSkeletonPose *pPose, int nBone );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAInterpolator : public CAnimator
{
	OBJECT_BASIC_METHODS(CAInterpolator);

public:
	CPtr<CAFunction> pFunc; // interpolation function
	CPtr<CAnimator> pAnim1; // animation 1
	CPtr<CAnimator> pAnim2; // animation 2
	
	CAInterpolator() {}
	CAInterpolator( CAnimator *_pAnim1, CAnimator *_pAnim2, CAFunction *_pFunc ) :
		pFunc( _pFunc ), pAnim1( _pAnim1 ), pAnim2( _pAnim2 ) {}

	void SetFunction( CAFunction *_pFunc ) { pFunc = _pFunc; }
	void SetInput1( CAnimator *pAnim ) { pAnim1 = pAnim; }
	void SetInput2( CAnimator *pAnim ) { pAnim2 = pAnim; }

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAAdder : public CAnimator
{
	OBJECT_BASIC_METHODS(CAAdder);

public:
	CPtr<CAFunction> pFunc; // interpolation function
	CPtr<CAnimator> pAnim1; // animation 1
	CPtr<CAnimator> pAnim2; // animation 2

	CAAdder() {}

	void SetFunction( CAFunction *_pFunc ) { pFunc = _pFunc; }
	void SetInput1( CAnimator *pAnim ) { pAnim1 = pAnim; }
	void SetInput2( CAnimator *pAnim ) { pAnim2 = pAnim; }

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CASubtractor : public CAnimator
{
	OBJECT_BASIC_METHODS(CASubtractor);
public:
	CASubtractor() {}

	CPtr<CAnimator> pAnim1; // animation 1
	CPtr<CAnimator> pAnim2; // animation 2
	STime t1, t2;

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CATurnBody : public CAnimator
{
	OBJECT_BASIC_METHODS(CATurnBody);
	float fAngle; // angle of turn
public:
	CATurnBody( float _fAngle = 0 ) : fAngle(_fAngle), nBoneSpine(-1), nBoneChest(-1) {}

	CPtr<CAFunction> pFunc; // add coefficient
	CPtr<CAnimator> pAnim; // animation
	int nBoneSpine;
	int nBoneChest;

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIK2Chain : public CAnimator
{
	OBJECT_BASIC_METHODS(CAIK2Chain);

public:
	int nBone1, nBone2, nBone3;
	CPtr<CAnimator> pInput;
	CPtr<CAnimator> pTarget; // target

	CAIK2Chain() {}
	CAIK2Chain( CAnimator *_pInput, CAnimator *_pTarget, int _nBone1, int _nBone2, int _nBone3 ) :
		nBone1(_nBone1), nBone2(_nBone2), nBone3(_nBone3), pInput(_pInput), pTarget(_pTarget) {}

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAPoseMemorizer : public CAnimator
{
	OBJECT_BASIC_METHODS(CAPoseMemorizer);
	SSkeletonPose memory;
public:
	CPtr<CAnimator> pInput;
	STime tMemory;
	STime tActive;

	virtual bool NeedUpdate( STime t ) { return false; }
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EIdleSources
{
	E_INTERNAL_IDLE_OFF = 1,
	E_NO_IDLE_ON_ACTION = 2,
	E_NO_IDLE_WHEN_STUNNED = 4,
	E_BREATH_ONLY_IDLE = 8,
	E_CUSTOM_ANIMATION_IDLE = 16
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSkeletonState
{
	ZDATA
	//bool bValid;
	char cIdleBannedFlags;
	int nAnimFlagsPoseWeapon;
	string szParams;
	CVec3 pos;
	float fAngle;
	bool bCrawl;
	CPtr<ITerrainFunction> pTerrain;
	int nAnimFlagsClassSex;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CAnimation> pIdleAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&cIdleBannedFlags); f.Add(3,&nAnimFlagsPoseWeapon); f.Add(4,&szParams); f.Add(5,&pos); f.Add(6,&fAngle); f.Add(7,&bCrawl); f.Add(8,&pTerrain); f.Add(9,&nAnimFlagsClassSex); f.Add(10,&pSide); f.Add(11,&pIdleAnimation); return 0; }
	//SSkeletonState() { bValid = false; }
	//
	bool IsIdleBanned() const { return ( cIdleBannedFlags & 7 ) > 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkeletonAnimator;
class CASmartAimer : public CAnimator
{
	OBJECT_BASIC_METHODS(CASmartAimer);

	NDb::CAnimation* GetIdleAnimation( const SSkeletonState &state );
	enum EFreezeSteady
	{
		FS_NONE,
		FS_FREEZE,
		FS_FROZEN
	};

	ZDATA_(CAnimator)
	SSkeletonPose current;
	STime tCurrent;

	CPtr<CAnimator> pIdleAnim;
	STime tIdleEnd;
	EFreezeSteady frost;
	// STime tIdleStart;
public:
	CPtr<CSkeletonAnimator> pAnimator;
	CDBPtr<NDb::CSkeleton> pSkeleton;
	
	CDGPtr< CFuncBase<SSkeletonPose> > pNextPose;
	CDGPtr< CFuncBase<SSkeletonState> > pState;
	CDGPtr< CFuncBase<STime> >  pNextPoseTime;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&current); f.Add(3,&tCurrent); f.Add(4,&pIdleAnim); f.Add(5,&tIdleEnd); f.Add(6,&frost); f.Add(7,&pAnimator); f.Add(8,&pSkeleton); f.Add(9,&pNextPose); f.Add(10,&pState); f.Add(11,&pNextPoseTime); return 0; }

	CASmartAimer() : frost(FS_NONE) { tCurrent = 0; /*tIdleStart = 0;*/ tIdleEnd = 0; }
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	virtual bool NeedUpdate( STime t );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAPoseAimer : public CAnimator
{
	OBJECT_BASIC_METHODS(CAPoseAimer);
	SSkeletonPose current;
	STime tCurrent;
public:
	CDGPtr< CFuncBase<SSkeletonPose> > pNextPose;
	CDGPtr< CFuncBase<STime> >  pNextPoseTime;

	CAPoseAimer() { tCurrent = 0; }
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CARandom : public CAnimator
{
	OBJECT_BASIC_METHODS(CARandom);
	ZDATA
	vector<CQuat> rotations;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&rotations); return 0; }
public:
	CARandom( int nBones = 0 );
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CABoneFilter : public CAnimator
{
	OBJECT_BASIC_METHODS(CABoneFilter);
public:
	CDGPtr< CFuncBase<SSkeletonPose> > pSource;
	int nIndex;

	CABoneFilter() {}
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleSkeleton;
////////////////////////////////////////////////////////////////////////////////////////////////////
// main animator for unit skeleton pose
class CSkeletonAnimator : public CFuncBase<SSkeletonPose>
{
	OBJECT_BASIC_METHODS(CSkeletonAnimator);
	ZDATA
	CPtr<CASequence> pSeq;
	int nBones;
public:
	CDGPtr< CPtrFuncBase<CFileSkeletonInfo> > pSkeleton;
	CDGPtr< CFuncBase<STime> > pTime;
	bool bServer;
	bool bItem;
private:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSeq); f.Add(3,&nBones); f.Add(4,&pSkeleton); f.Add(5,&pTime); f.Add(6,&bServer); f.Add(7,&bItem); return 0; }

	CParticleSkeleton* GetCorpseAnimator();

protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	CSkeletonAnimator() {}
	CSkeletonAnimator( NDb::CSkeleton *_pSkeleton );

	bool IsFreezed() const;
	const int GetBoneIndex( const char *pszName );
	void GetCurrentBonePos( int nIndex, SBonePose *pBone );
	void GetDefaultBonePos( int nIndex, SBonePose *pBone );

	CAnimation* CreateAnimation( NDb::CAnimation *pA, STime tStart, bool bCycle = false );

	void AddAnimator( STime tFrom, CAnimator *pAnim );
	void AddTransit( STime tFrom, STime tTo, CAnimator *pAnim, ETransitType trType = LINEAR );
	void AddMemorizer( STime tActive );
	void AddAimer( STime tFrom, CFuncBase<SSkeletonPose> *pNextPose, CFuncBase<STime> *pNextPoseTime );
	void AddSmartAimer( STime tFrom, CFuncBase<SSkeletonPose> *pNextPose,
		CFuncBase<SSkeletonState> *pState, CFuncBase<STime> *pNextPoseTime,
		CSkeletonAnimator *pAnimator, NDb::CSkeleton *pSkeleton );
	void AddRandom( STime tFrom, STime tTo );
	
	void AddBoneFilter( STime tFrom, CFuncBase<SSkeletonPose> *pSource, int nAddBone );

	// retail @0xdd4e0 9-arg form: + ptWhereImpact (the impulse falloff origin -- Die passes the raw-cast
	// SUnitPosition bytes, an ORIGINAL BUG), + pServer (-> CParticleSkeleton::pUnit), + bFall
	// (fDeathFall > 0: hands collide immediately). ptWhereImpact/bFall are stored only on the
	// non-effector (real ragdoll) branch, like retail.
	void AddDynamics( STime tFrom, STime tMaxAnim, const CVec3 &impact, const CVec3 &ptWhereImpact,
		NAI::IAIMap *pMap, NGScene::CCInt *pFloor, NWorld::CUnitServer *pServer,
		CAnimator *pEffector = 0, bool bFall = false );
	bool IsInstableCorpse();
	bool CalmCorpse();
	// later
	void AddSimpleIK( STime tFrom, const char *pszBoneName, CAnimator *pEffector );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// additional information about skeleton pose (useful for CASmartAimer)
class CSkeletonState: public CFuncBase<NAnimation::SSkeletonState>
{
	OBJECT_BASIC_METHODS(CSkeletonState);
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	ZDATA
	NAnimation::SSkeletonState state;
	CDGPtr< CFuncBase<STime> > pTime;
	CPtr< NAnimation::CSkeletonAnimator > pAnimator;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&state); f.Add(3,&pTime); f.Add(4,&pAnimator); return 0; }

	CSkeletonState() {} //{ state.bValid = true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// filter for additional bones
class CAddBoneFilter : public CFuncBase<SFBTransform>
{
	OBJECT_BASIC_METHODS(CAddBoneFilter);
	ZDATA
	int nAddBone;
	CDGPtr< CFuncBase<SSkeletonPose> > pAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nAddBone); f.Add(3,&pAnimation); return 0; }
protected:
	virtual bool NeedUpdate() { return pAnimation.Refresh(); }
	virtual void Recalc();
public:
	CAddBoneFilter() : nAddBone(0) {}
	CAddBoneFilter( CFuncBase<SSkeletonPose> *_pAnim, int _nAddBone = 0 ) : pAnimation(_pAnim), nAddBone(_nAddBone) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// additional bones locators
class CAddBoneLocators : public CFuncBase<SSkeletonPose>
{
	OBJECT_BASIC_METHODS(CAddBoneLocators);
	int nAddBone;
protected:
	virtual bool NeedUpdate() { return pAnimation.Refresh() | pLocators.Refresh(); }
	virtual void Recalc();
public:
	CDGPtr< CPtrFuncBase<CFileSkeletonInfo> > pLocators;
	CDGPtr< CFuncBase<SSkeletonPose> > pAnimation;

	CAddBoneLocators() {}
	CAddBoneLocators( int _nAddBone, NDb::CGeometry *pGeometry );

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* PrecacheAnimation( NDb::CAnimation *_pA );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif