#ifndef __GANIMPARTICLES_H_
#define __GANIMPARTICLES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GAnimBase.h"
#include "..\DBFormat\DataPhys.h"   // complete NDb::CDBPhysParams for CASphereSet::pPhys (CDBPtr needs full type)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class IAIMap;
}
namespace NDb { class CDBPhysParams; }      // fwd for CASphereSet::pPhys (release save-format)
namespace NWorld { class CUnitServer; }     // fwd for CParticleSkeleton::pUnit (release save-format)
namespace NGScene
{
	class CCInt;
}
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAParticle : public CAnimator
{
public:
	struct SStick
	{
		int nP1;
		int nP2;
		float fRest;
		int nMax;
	};
protected:
	vector<int> nParticleBones; // correspondence nParticle -> nSkeletonBone
	vector<CVec3> particles; // current particle positions
	struct SRestoreBone
	{
		int nP1;
		int nP2;
		int nP3;
		SRestoreBone( int a = -1, int b = -1, int c = -1 ) { nP1 = a; nP2 = b; nP3 = c; }
	};
	vector<SRestoreBone> restore; // for restoring bones from particles
	vector<SStick> sticks; // stick pairs

	void InitSticks( const int *pPairs, int nPairs );
	void IterateSticks();
	void RestoreTriangle( vector<CVec3> parts, int nP1, int nP2, int nP3, int nTargetBone, SSkeletonPose *pPose );
	void RestoreLine( vector<CVec3> parts, int nP1, int nP2, int nTargetBone, SSkeletonPose *pPose );
	void RestoreAll( vector<CVec3> parts, SSkeletonPose *pPose );
public:
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleSkeleton : public CAParticle
{
	OBJECT_BASIC_METHODS(CParticleSkeleton);
	ZDATA_(CAParticle)
	STime tCurrent;
	vector<CVec3> last;
	vector<SStick> softs;
	vector<float> fInvMasses;
	vector<CVec3> spine;
	int nStep;
	bool bStopped;
	bool bStartPhys;
	SSkeletonPose defaultPose;

public:
	CPtr<NAI::IAIMap> pMap;
	CPtr<CAnimator> pInput;
	STime tActive, tMax;
	CVec3 impact;

	CPtr<CAnimator> pEffector;
private:
	CObj<NGScene::CCInt> pFloor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAParticle*)this); f.Add(2,&tCurrent); f.Add(3,&last); f.Add(4,&softs); f.Add(5,&fInvMasses); f.Add(6,&spine); f.Add(7,&nStep); f.Add(8,&bStopped); f.Add(9,&bStartPhys); f.Add(10,&defaultPose); f.Add(11,&pUnit); f.Add(12,&addRandom); f.Add(13,&pMap); f.Add(14,&pInput); f.Add(15,&tActive); f.Add(16,&tMax); f.Add(17,&impact); f.Add(18,&pEffector); f.Add(19,&pFloor); f.Add(20,&ptWhereImpact); f.Add(21,&bIsFalling); return 0; }
	CPtr<NWorld::CUnitServer> pUnit;
	vector<CQuat> addRandom;
	CVec3 ptWhereImpact;
	bool bIsFalling = false;

	void Step();
	void Init( STime t, SSkeletonPose &pose, SSkeletonPose &nextPose, const CVec3 &impact );
	void CalcParticles( vector<CVec3> *pParticles, SSkeletonPose &pose );
	void CheckIfCollided( STime t, SSkeletonPose *pPose, SSkeletonPose *pNextPose );
public:
	CParticleSkeleton() {}
	CParticleSkeleton( NGScene::CCInt *_pFloor, CPtrFuncBase<CFileSkeletonInfo> *_pSkeleton );
	virtual bool NeedUpdate( STime t );
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CASphereSet : public CAnimator
{
	OBJECT_BASIC_METHODS(CASphereSet);
	ZDATA_(CAnimator)
	vector<SMassSphere> spheres;
	CVec3 boundSize;
	
	float fMass;
	CVec3 massCenter;
	SHMatrix inertiaInvBody;
	float fBoundSize;

	CVec3 pos;
	CQuat rot;
	CVec3 p; // linear moment
	CVec3 l; // angular moment

	CVec3 lastPos; // for interpolation
	CQuat lastRot;
	STime tCurrent;
	STime tLast; // for case - finish time

	bool bStopped; // is resting
	bool bCollided;
public:
	CDGPtr< CFuncBase<STime> > pTime;
	CPtr<NAI::IAIMap> pMap;

	float fResistance;
	float fFriction;
private:
	int nFloor;
	// release save-format: insert pPhys(18)/pIgnore(19) before pTime/pMap (now 20/21), DROP
	// fResistance/fFriction from serialize (members kept), nFloor stays 22, append the existing
	// posVel(23)/rotVel(24)/inertiaInv(25). Renumber independently decode-verified. Dead-in-dev.
	CDBPtr<NDb::CDBPhysParams> pPhys;
	CPtr<CObjectBase> pIgnore;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&spheres); f.Add(3,&boundSize); f.Add(4,&fMass); f.Add(5,&massCenter); f.Add(6,&inertiaInvBody); f.Add(7,&fBoundSize); f.Add(8,&pos); f.Add(9,&rot); f.Add(10,&p); f.Add(11,&l); f.Add(12,&lastPos); f.Add(13,&lastRot); f.Add(14,&tCurrent); f.Add(15,&tLast); f.Add(16,&bStopped); f.Add(17,&bCollided); f.Add(18,&pPhys); f.Add(19,&pIgnore); f.Add(20,&pTime); f.Add(21,&pMap); f.Add(22,&nFloor); f.Add(23,&posVel); f.Add(24,&rotVel); f.Add(25,&inertiaInv); return 0; }

	CVec3 posVel; // from p
	CVec3 rotVel; // from l
	SHMatrix inertiaInv; //from l

	void CalcVelocities();
	void CalcPosVel();
	void CalcRotVel();
	void AdvanceIntegrator( float fTime, bool bMidPoint = false );
	CVec3 PredictPosition( float fTime, bool bMidPoint = false );
	void Step();
	float GetEnergy();
	void ApplyCollision( CVec3 ptColl, CVec3 vel );
	void AddSphere( const SSphere &sphere, float fMass );
public:
	CASphereSet( int _nFloor = -100 );
	
	void InitSpheres( const vector<SMassSphere> &_spheres );
	void InitBound( const CVec3 center, const CVec3 size );
	void Init( STime t, const CVec3 &pos, const CQuat &rot, const CVec3 &vel, bool bMassCenter = false );
	bool HasStopped();
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	bool DidCollide() { return bCollided; } 
	void Calc( STime t );
	int GetFloor() const { return nFloor; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CATrailPath: public CAnimator
{
	OBJECT_BASIC_METHODS(CATrailPath);
public:
	struct STrailPoint
	{
		ZDATA
		CVec3 vDir;
		CVec3 vPosition;
		STime sPassTime;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vDir); f.Add(3,&vPosition); f.Add(4,&sPassTime); return 0; }
	};
private:
	ZDATA_(CAnimator)
	int nTrailCount;
	STime sCast;
	vector<STrailPoint> trailpointsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&nTrailCount); f.Add(3,&sCast); f.Add(4,&trailpointsSet); return 0; }

public:
	CATrailPath() {}

	void Init( STime sCast, const vector<STrailPoint> &trail );
	void GetFrame( STime sFrame, SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif