#ifndef __IMAPEDITOR_H_
#define __IMAPEDITOR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Camera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLightPrefs
{
	CVec3 ptLight;
	CVec3 ptOrigin;
	bool  bShadows;
	CVec3 vAmbientColor, vLightColor, vGlossColor, vFogColor, vVapourColor;
	float fFogStart, fFogDistance, fVapourHeight, fVapourDensity;
	int   nSkyID;
	CVec3 vShadowColor;
	float fVapourNoiseParam;
	float fVapourSpeed;
	float fVapourSwitchTime;
	CVec3 vBackColor;
	CVec3 vGroundAmbientColor;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ERPGItemCamera 
{ 
	CAM_INVENTORY, 
	CAM_SLOT, 
	CAM_AMMO 
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICMapEditor: public NMainLoop::CInterfaceCommand
{
public:
  enum EViewType
  {
    VIEW_PLACEMENT,
    VIEW_MODEL,
    VIEW_TEXTURE,
    VIEW_GEOMETRY,
    VIEW_MATERIAL,
		VIEW_ANIMATION,
		VIEW_RNDMODEL,
		VIEW_CONTAINER,
		VIEW_PARTICLES,
		VIEW_AIMODEL,
		VIEW_SOUND,
		VIEW_RPGITEM,
		VIEW_INTERFACE,
		VIEW_HEAD,
		VIEW_OBJECT,
		VIEW_PERS,
		VIEW_SOUNDEFFECT,
		VIEW_CONSTRUCTION_PART,
  };
	struct SMapParams
	{
		CVec3       ptCameraPos;
		EViewType   nView;
		int         nObjectID;
		SLightPrefs lightPrefs;
		int 				nExtra;
		bool				bCameraReset;
		float				fFOV;
		vector<string> szParams;
		ICamera::SCameraPos camera;
	};
private:
  OBJECT_BASIC_METHODS(CICMapEditor);
  SMapParams  mapParams;
	bool bBuildMap;
	HWND hWnd;
public:
	CICMapEditor() {}
  CICMapEditor( const SMapParams &params, HWND hWnd, bool bBuildMap = true );
  virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIMapEditor: public NMainLoop::IInterfaceBase
{
public:
	virtual void GetCameraPos( ICamera::SCameraPos *pPos ) const {}
	virtual void SetCamera( const ICamera::SCameraPos &cameraPos ) {};
	virtual int  GetSelectionMask() const { return 0; } // EBrushType mask
	virtual void SaveHitTestObject() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif