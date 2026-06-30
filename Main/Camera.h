#ifndef __CAMERA_H_
#define __CAMERA_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
////////////////////////////////////////////////////////////////////////////////////////////////////
const float	F_FOV = 35;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
	struct SEvent;
}
enum ECameraType
{
	CAMERA_PC,
	CAMERA_MAYA,
	CAMERA_FIRSTPERSON
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera: public CObjectBase
{
public:
  struct SCameraPos
  {
    float fRod;
    float fPitch, fYaw, fRoll;
		float fFOV;
    CVec3 ptAnchor;
		SCameraPos() { memset( this, 0, sizeof(*this) ); fFOV = F_FOV; }
		SCameraPos( CVec3 _ptAnchor, float _fRod, float _fPitch, float _fYaw, float _fRoll, float _fFOV = F_FOV ) :
			ptAnchor( _ptAnchor ), fRod( _fRod ), fPitch( _fPitch ), fYaw( _fYaw ), fRoll( _fRoll ), fFOV( _fFOV ) {}
  };
	struct SCameraLimits
	{
		float fMinRod, fMaxRod, fMinPitch, fMaxPitch;
		CTRect<float> sZoneLimit;

		SCameraLimits(): fMinRod(-1000), fMaxRod(1000), fMinPitch( -FP_2PI ), fMaxPitch( FP_2PI ), sZoneLimit( -1000, -1000, 1000, 1000 )  {}
		SCameraLimits( float _fMinRod, float _fMaxRod, float _fMinPitch, float _fMaxPitch, const CTRect<float> &_sZoneLimit )
			: fMinRod(_fMinRod), fMaxRod(_fMaxRod), fMinPitch(_fMinPitch), fMaxPitch(_fMaxPitch), sZoneLimit( _sZoneLimit ) {}
	};

public:
	virtual CVec3 GetForwardDir() const = 0;
	virtual CVec3 GetStrafeDir() const = 0;

	virtual CVec3 GetCP() const = 0;
	virtual SHMatrix GetPos() const = 0;
	virtual void GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const = 0;
	// target rect for correct center position, in [0,1] range
	virtual const CTRect<float>& GetScreenRect() const = 0;
	virtual void SetScreenRect( const CTRect<float> &sRect ) = 0;

	virtual float GetFOV() const = 0;
	virtual void SetFOV( float fFOV ) = 0;

	// near/far clip planes (release added these; the CameraSetClipping script binding -> CUICmdSetCameraClipDistance
	// drives them). Default no-op so non-CBaseCamera implementations need not override.
	virtual void SetClipDistance( float fMin, float fMax ) {}
	virtual void GetClipDistance( float *pMin, float *pMax ) const {}

  virtual void GetPlacement( SCameraPos *pPlacement ) const = 0;
	virtual void SetPlacement( const SCameraPos &sPlacement ) = 0;

	virtual void GetLimits( SCameraLimits *pLimits ) const {}
	virtual void SetLimits( const SCameraLimits &sLimits ) {}

	virtual void Update( const STime &sTime ) = 0;
	virtual void ProcessEvent( const NInput::SEvent &eEvent ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CreateCamera( ECameraType eType = CAMERA_PC, float fCameraSpeed = 1.0f );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif