#ifndef __A5_IMISSION_EXEC_H__
#define __A5_IMISSION_EXEC_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld { class CUICmdCameraLocator; class CUICmdUnitCamera; class CUICmdPointCamera; class CUICmdScriptMoveCamera; }
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExec
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExec: public CObjectBase
{
	ZDATA
	CPtr<NWorld::CUICmd> pCmd;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCmd); return 0; }

	CUICmdExec() {}
	CUICmdExec( NWorld::CUICmd *_pCmd ): pCmd( _pCmd ) {}

	virtual bool Update( const STime &sTime ) = 0;
	virtual void Cancel() {}
	virtual void Finished() {}

	NWorld::CUICmd* GetCmd() const { return pCmd; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecContainer: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecContainer );
	ZDATA
	ZPARENT( CUICmdExec );
	list< CPtr<CUICmdExec> > commands;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmdExec *)this); f.Add(3,&commands); return 0; }
	//
public:
	CUICmdExecContainer() {}
	CUICmdExecContainer( NWorld::CUICmd *_pCmd ): CUICmdExec( _pCmd ) {}
	//
	void Add( CUICmdExec *pCmd );
	virtual bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdLocatorExec -- release-new base of the camera-command executors (iUIExec).
// Wraps a single NWorld::CUICmdCameraLocator script command and exposes its move
// priority. Abstract (no Update override): added as additive, behaviour-neutral
// parity surface -- the move/unit/explosion executors are NOT re-parented onto it
// in this pass (see converge notes), so it is never instantiated, registered, or
// serialized here. The implicit copy ctor (member-wise CPtr copy of pCmd + pLocator)
// reproduces release CUICmdLocatorExec::CUICmdLocatorExec @0x24f8f0 exactly.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdLocatorExec: public CUICmdExec
{
protected:
	ZDATA_(CUICmdExec)
	CPtr<NWorld::CUICmdCameraLocator> pLocator;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdExec*)this); f.Add(2,&pLocator); return 0; }

	CUICmdLocatorExec() {}
	CUICmdLocatorExec( NWorld::CUICmdCameraLocator *_pLocator );

	virtual int GetPriority() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdMoveCameraExec (retail iUIExec.obj, 84 bytes, saveload id 0x50412162; ctor @0x24eee0,
// SetTarget @0x24e9f0, Update @0x24e840, Finished @0x24e6a0, operator& @0x24fd80). W5: converged
// to the retail shape -- derives CUICmdLocatorExec, wraps ONE NWorld::CUICmdScriptMoveCamera and
// morphs the camera through its waypoint list, sTransitionTime per waypoint (the arbitrated
// pExecLocator sink; CreateCameraExecutor dispatches ScriptMoveCamera here like retail @0x24f150).
// (The dev single-target CUICmdMoveCamera wrapper + CUICmdFollowCameraExec/CUICmdRestoreCameraExec
// died with the dev-only NWorld UICmds -- retail has no follow/restore execs.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdMoveCameraExec: public CUICmdLocatorExec
{
	OBJECT_BASIC_METHODS(CUICmdMoveCameraExec)
private:
	ZDATA_(CUICmdLocatorExec)
	CPtr<IMission> pMission;                        // +0x14 tag 2
	CObj<NWorld::CUICmdScriptMoveCamera> pCmd;      // +0x18 tag 3
	STime sMorphTime;                               // +0x1c tag 4: the morph's start time (0 = first Update)
	STime sTransitionTime;                          // +0x20 tag 5: PER-WAYPOINT transition time (0 = done/instant)
	ICamera::SCameraPos sCameraPos;                 // +0x24 tag 6 (raw 0x20): the current segment's start pose
	vector<ICamera::SCameraPos> vTargetPositions;   // +0x44 tag 7 (DoDataVector): normalized waypoints
	int iLastCamera;                                // +0x50 tag 8: last segment index (start pose re-sampled on change)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdLocatorExec*)this); f.Add(2,&pMission); f.Add(3,&pCmd); f.Add(4,&sMorphTime); f.Add(5,&sTransitionTime); f.Add(6,&sCameraPos); f.Add(7,&vTargetPositions); f.Add(8,&iLastCamera); return 0; }   // retail @0x24fd80

private:
	void NormalizeAngle( float *pfAngle );
	void NormalizePos( ICamera::SCameraPos *pPos );
	// retail @0x24e9f0: adopt + fmod-normalize the waypoints, store the per-waypoint transition
	void SetTarget( const vector<ICamera::SCameraPos> &positions, STime _transitionTime );

public:
	CUICmdMoveCameraExec(): sMorphTime( 0 ), sTransitionTime( 0 ), iLastCamera( 0 ) {}
	CUICmdMoveCameraExec( NWorld::CUICmdScriptMoveCamera *pCmd, IMission *pMission );   // retail @0x24eee0

	bool Update( const STime &sTime );   // retail @0x24e840
	void Finished();                     // retail @0x24e6a0 (no Cancel override in retail)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdUnitCameraExec -- sink for the arbitrated NWorld::CUICmdUnitCamera (retail @0x24eae0). Frames the
// shooter (+ optional target) with priority-keyed gating, holds ~4 s, then self-terminates. GetPriority()
// feeds the mission's pExecLocator arbitration (MustReplaceCameraExecutor). Derives CUICmdExec directly (a5dll
// only routes CUICmdUnitCamera through the locator slot) and exposes GetPriority() from the wrapped command.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdUnitCameraExec: public CUICmdLocatorExec
{
	OBJECT_BASIC_METHODS( CUICmdUnitCameraExec )
private:
	ZDATA_(CUICmdLocatorExec)
	CPtr<IMission> pMission;              // +0x14
	CPtr<NWorld::CUICmdUnitCamera> pCmd;  // +0x18
	CPtr<ICamera> pLockedCamera;          // +0x1c  (ctor Lock(1) / Cancel|Finished Lock(0))
	bool  bDone;                          // +0x20
	STime tStart;                         // +0x24
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdLocatorExec*)this); f.Add(2,&pMission); f.Add(3,&pCmd); f.Add(4,&pLockedCamera); f.Add(5,&bDone); f.Add(6,&tStart); return 0; }

	CUICmdUnitCameraExec(): bDone( false ), tStart( 0 ) {}
	CUICmdUnitCameraExec( NWorld::CUICmdUnitCamera *pCmd, IMission *pMission );

	bool Update( const STime &sTime );
	void Cancel();
	void Finished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExplosionCameraExec -- sink for the arbitrated NWorld::CUICmdPointCamera (retail iUIExec.obj,
// 0x44 bytes, saveload id 0x00183130; ctor @0x24f570, Update @0x24e6e0, Finished @0x24f550). Frames
// the blast point via ShowPlacesFromBestPoint (rod >= 12, the -100 nFloor sentinel resolves to the
// current cut floor), dwells nExecTime seconds, then self-terminates. Scroll-locks the camera.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExplosionCameraExec: public CUICmdLocatorExec
{
	OBJECT_BASIC_METHODS( CUICmdExplosionCameraExec )
private:
	ZDATA_(CUICmdLocatorExec)
	CPtr<IMission> pMission;               // retail +0x14
	CPtr<NWorld::CUICmdPointCamera> pCmd;  // (retail copies the tuning fields; kept via the command)
	CPtr<ICamera> pLockedCamera;           // retail +0x24
	bool  bDone;                           // retail +0x28
	STime tStart;                          // retail +0x2c
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdLocatorExec*)this); f.Add(2,&pMission); f.Add(3,&pCmd); f.Add(4,&pLockedCamera); f.Add(5,&bDone); f.Add(6,&tStart); return 0; }

	CUICmdExplosionCameraExec(): bDone( false ), tStart( 0 ) {}
	CUICmdExplosionCameraExec( NWorld::CUICmdPointCamera *pCmd, IMission *pMission );

	bool Update( const STime &sTime );
	void Cancel();
	void Finished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MustReplaceCameraExecutor( int nCur, int nNew );
bool IsVisibleByActivePlayer( NWorld::CUnit *pUnit, IMission *pMission );
//
CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd, IMission *pMission );
// release CreateCameraExecutor @0x24f150: RTTI-dispatch a camera-locator command (CUICmdUnitCamera) to its
// executor, for the mission's dedicated pExecLocator arbitration slot.
CUICmdLocatorExec* CreateCameraExecutor( NWorld::CUICmdCameraLocator *pCmd, IMission *pMission );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif