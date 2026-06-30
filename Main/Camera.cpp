#include "StdAfx.h"
#include "Camera.h"
#include "..\Misc\Geom.h"
#include "Transform.h"
#include "..\Input\Bind.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseCamera: public ICamera
{
protected:
	NInput::CBind fwd, strafe, zoom, pitch, rotate;
	ZDATA
  SCameraPos sPlacement;
	SCameraLimits sLimits;
	CTRect<float> sScreenRect;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sPlacement); f.Add(3,&sLimits); f.Add(4,&sScreenRect); return 0; }
	// NOTE: full CBaseCamera save-format parity (release adds nLockCount/nCutFloor/clip + a widened
	// SCameraLimits 32->56) is the separate, still-BLOCKED SCameraLimits ripple leg. These clip members are
	// runtime-only here (the menu camera is never saved); they are intentionally NOT in operator&.
	float fMinClipDistance;
	float fMaxClipDistance;

public:
	CBaseCamera();

	void GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const;

	const CTRect<float>& GetScreenRect() const;
	void SetScreenRect( const CTRect<float> &sRect );

	float GetFOV() const;
	void SetFOV( float fFOV );

	void SetClipDistance( float fMin, float fMax ) { fMinClipDistance = fMin; fMaxClipDistance = fMax; }
	void GetClipDistance( float *pMin, float *pMax ) const { *pMin = fMinClipDistance; *pMax = fMaxClipDistance; }

  void GetPlacement( SCameraPos *pPlacement ) const;
	void SetPlacement( const SCameraPos &sPlacement );

	void GetLimits( SCameraLimits *pLimits ) const;
	void SetLimits( const SCameraLimits &sLimits );

	void ProcessEvent( const NInput::SEvent &eEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBaseCamera::CBaseCamera():
	sScreenRect( 0, 0, 1, 1 ),
	fwd( "camera_forward" ), strafe( "camera_strafe" ), zoom( "camera_zoom" ), pitch ( "camera_pitch" ), rotate( "camera_rotate" )
{
	sPlacement.ptAnchor = VNULL3;
	sPlacement.fRod = 30;
	sPlacement.fPitch = ToRadian( -70.0f ); 
	sPlacement.fYaw = 0;
	sPlacement.fRoll = 0;
	sPlacement.fFOV = F_FOV;
	fMinClipDistance = 0.1f;		// release CBaseCamera ctor defaults (matches the old hardcoded MakeProjective values)
	fMaxClipDistance = 100;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const
{
	float fShiftX = ( sScreenRect.x1 + sScreenRect.x2 ) - 1;
	float fShiftY = ( sScreenRect.y1 + sScreenRect.y2 ) - 1;
	pTS->MakeProjective( vScreenSize, sPlacement.fFOV, fMinClipDistance, fMaxClipDistance, CVec2( fShiftX, -fShiftY ) );
	pTS->SetCamera( GetPos() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTRect<float>& CBaseCamera::GetScreenRect() const
{
	return sScreenRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetScreenRect( const CTRect<float> &sRect )
{
	sScreenRect = sRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CBaseCamera::GetFOV() const
{
	return sPlacement.fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetFOV( float _fFOV )
{
	sPlacement.fFOV = _fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetPlacement( SCameraPos *pPlacement ) const
{
	ASSERT( pPlacement );
	*pPlacement = sPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetPlacement( const SCameraPos &_sPlacement )
{
	sPlacement = _sPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetLimits( SCameraLimits *pLimits ) const
{
	ASSERT( pLimits );
	*pLimits = sLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetLimits( const SCameraLimits &_sLimits )
{
	sLimits = _sLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::ProcessEvent( const NInput::SEvent &eEvent )
{
	fwd.ProcessEvent( eEvent );
	strafe.ProcessEvent( eEvent );
	zoom.ProcessEvent( eEvent );
	pitch.ProcessEvent( eEvent );
	rotate.ProcessEvent( eEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CCamera);
private:
	ZDATA_(CBaseCamera)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseCamera*)this); return 0; }

public:
	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X ) * CQuat( sPlacement.fRoll, V3_AXIS_Y );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetCP() const
{
	return sPlacement.ptAnchor - GetForwardDir() * sPlacement.fRod;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::Update( const STime &sTime )
{
	sPlacement.fPitch += pitch.GetDelta();
	sPlacement.fYaw -= rotate.GetDelta();
	float fFwd = fwd.GetDelta() * 10.0f;
	float fStrafe = strafe.GetDelta() * 10.0f;
	float fZoom = zoom.GetDelta() * 10.0f;
	//ASSERT( fabs(fZoom) < 5 );
	if ( fZoom < -5 )
		fZoom = -5;
	if ( fZoom > 5 )
		fZoom = 5;
	//
	CVec3 ptDir( GetForwardDir() );
	// fix anchor.z
	if ( fabs( ptDir.z ) > 0.1f )
	{
		float fFix = sPlacement.ptAnchor.z / ptDir.z;
		sPlacement.fRod -= fFix;
    sPlacement.ptAnchor -= ptDir * fFix;
	}
	// forward
	ptDir.z = 0;
	if ( fabs2( ptDir ) > 0 )
	{
		Normalize( &ptDir );
		sPlacement.ptAnchor += ptDir * fFwd;
	}
	// strafe
	ptDir = GetStrafeDir();
	ptDir.z = 0;
	if ( fabs2( ptDir ) > 0 )
	{
		Normalize( &ptDir );
		sPlacement.ptAnchor += ptDir * fStrafe;
	}
	// zoom
	sPlacement.fRod += fZoom;
	// sLimits
	sPlacement.fRod = Clamp( sPlacement.fRod, sLimits.fMinRod, sLimits.fMaxRod );
	sPlacement.fPitch = Clamp( sPlacement.fPitch, sLimits.fMinPitch, sLimits.fMaxPitch );
	sPlacement.ptAnchor.x = Clamp( sPlacement.ptAnchor.x, sLimits.sZoneLimit.x1, sLimits.sZoneLimit.x2 );
	sPlacement.ptAnchor.y = Clamp( sPlacement.ptAnchor.y, sLimits.sZoneLimit.y1, sLimits.sZoneLimit.y2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FirstPerson camera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFPCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CFPCamera);
private:
	ZDATA_(CBaseCamera)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseCamera*)this); return 0; }

public:
	CFPCamera() {}

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetCP() const
{
	return sPlacement.ptAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CFPCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.ptAnchor, GetForwardDir() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float CAMERA_SPEED = 8.0f;
const float ROTATE_SPEED = 5.0f;
void CFPCamera::Update( const STime &sTime )
{
	sPlacement.fPitch += CAMERA_SPEED * pitch.GetDelta();
	sPlacement.fYaw -= CAMERA_SPEED * rotate.GetDelta();
	float fFwd = fwd.GetDelta() * CAMERA_SPEED;
	float fStrafe = strafe.GetDelta() * CAMERA_SPEED;
	float fZoom = zoom.GetDelta() * CAMERA_SPEED;
	//
	CVec3 ptDir( GetForwardDir() );
	Normalize( &ptDir );
	sPlacement.ptAnchor += ptDir * fFwd;

	ptDir = GetStrafeDir();
	Normalize( &ptDir );
	sPlacement.ptAnchor += ptDir * fStrafe;

	sPlacement.fRod += fZoom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Maya 
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMayaCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CMayaCamera);
private:
	NInput::CBind roll;
	ZDATA_(CBaseCamera)
	float fSpeed;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseCamera*)this); f.Add(2,&fSpeed); return 0; }

public:
	CMayaCamera( float fSpeed = 1 );

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void ProcessEvent( const NInput::SEvent &eEvent );
	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//																						MAYA camera
////////////////////////////////////////////////////////////////////////////////////////////////////
CMayaCamera::CMayaCamera( float _fSpeed ):
	fSpeed(_fSpeed), roll( "camera_roll" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X ) * CQuat( sPlacement.fRoll, V3_AXIS_Y );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetCP() const
{
	return sPlacement.ptAnchor - GetForwardDir() * sPlacement.fRod;
	return sPlacement.ptAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CMayaCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMayaCamera::ProcessEvent( const NInput::SEvent &eEvent )
{
	roll.ProcessEvent( eEvent );
	CBaseCamera::ProcessEvent( eEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float PITCH_SPEED = 15;
const float YAW_SPEED = 10;
const float ROLL_SPEED = 10;
void CMayaCamera::Update( const STime &sTime )
{
	sPlacement.fPitch += fSpeed * PITCH_SPEED * pitch.GetDelta();
	sPlacement.fYaw -= fSpeed * YAW_SPEED * rotate.GetDelta();
	sPlacement.fRoll -= fSpeed * ROLL_SPEED * roll.GetDelta();
	float fUp = fSpeed * fwd.GetDelta() * 2.0f;
	float fStrafe = fSpeed * strafe.GetDelta() * 2.0f;
	float fZoom = fSpeed * zoom.GetDelta() * (fabs(sPlacement.fRod) + 1) * 7.0f;
	//
	CVec3 ptMove( fStrafe, 0, fUp );
	if ( fabs2( ptMove ) > FP_EPSILON )
	{
		ptMove *= Max( fabs( sPlacement.fRod ), 2.0f );
		SHMatrix m;
		MakeMatrix( &m, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
		m.RotateHVector( &ptMove, ptMove );
		CVec3 ptFw = GetForwardDir();
		sPlacement.ptAnchor = ptMove + sPlacement.fRod * ptFw;
		sPlacement.fRod = Sign( sPlacement.fRod ) * fabs(sPlacement.ptAnchor - ptMove) / fabs( ptFw );
	}
	// zoom
	sPlacement.fRod += fZoom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CreateCamera( ECameraType eType, float fCameraSpeed )
{
	switch( eType )
	{
		case CAMERA_PC:
			return new CCamera;
		case CAMERA_FIRSTPERSON:
			return new CFPCamera;
		case CAMERA_MAYA:
			return new CMayaCamera( fCameraSpeed );
	}

	ASSERT(0);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0xF2221130, CCamera );
REGISTER_SAVELOAD_CLASS( 0xF2221131, CFPCamera );
REGISTER_SAVELOAD_CLASS( 0xA2912170, CMayaCamera );
