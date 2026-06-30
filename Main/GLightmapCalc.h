#ifndef __GLightmapCalc_H_
#define __GLightmapCalc_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GfxRender.h"
#include "GRenderCore.h"
#include "GScene.h"

namespace NGfx
{
	class CTexture;
}
namespace NGScene
{
class CMaterial;
class CPerMaterialCombiner;
struct SLightInfo;
const int N_CL_TEMP_REGISTER = 2;
const int N_CL_TARGET_REGISTER = 3;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGlobalIlluminationInfo
{
	struct SDirectional
	{
		CVec3 vColor, vDir;
		bool bIsRendered;

		SDirectional() {}
		SDirectional( const CVec3 &_vColor, const CVec3 &_vDir, bool _bIsRendered )
			: vColor(_vColor), vDir(_vDir), bIsRendered(_bIsRendered) {}
	};
	struct SPoint
	{
		CVec3 vColor, vCenter;
		float fRadius;
		bool bIsRendered;
		
		SPoint() {}
		SPoint( const CVec3 &_vColor, const CVec3 &_vCenter, float _fRadius, bool _bIsRendered )
			: vColor(_vColor), vCenter(_vCenter), fRadius(_fRadius), bIsRendered(_bIsRendered) {}
	};
	ZDATA
	SSphere globalBounds;
	CVec3 vAmbient;
	vector<SDirectional> parallel;
	vector<SPoint> points;
	CVec3 vUpDifColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&globalBounds); f.Add(3,&vAmbient); f.Add(4,&parallel); f.Add(5,&points); f.Add(6,&vUpDifColor); return 0; }
	
	SGlobalIlluminationInfo(): vAmbient(0,0,0), globalBounds(CVec3(0,0,0), 10) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// для расчета динамических lightmap`ов
class IGScene;
struct SLightStateCalcSeed
{
	int nSeed;

	SLightStateCalcSeed() : nSeed(0) {}
};
class CLightState
{
	void AddRay( const CVec3 &vFrom, const CVec3 &vDir, const CVec3 &vColor );
	void AddParallel( bool bDoRender, const SSphere &_bound, const CVec3 &vDir, const CVec3 &vColor );
	void AddPoint( bool bDoRender, const CVec3 &vCenter, float fRadius, const CVec3 &_vColor );
	void TraceDynamicLMPointLight( SDynamicAmbientInfo *pRes, const CVec3 &vTarget, float fTargetR, 
		const CVec3 &vCenter, float fRadius, const CVec3 &vColor, const CVec3 &vSemiNormal, IGScene *pVis ) const;
	CPtr<IGScene> pVis;
public:
	struct SParallelLight
	{
		CVec3 vColor, vDir;

		SParallelLight() {}
		SParallelLight( const CVec3 &_vColor, const CVec3 &_vDir )
			: vColor(_vColor), vDir(_vDir) {}
	};
	struct SSemiPointLight
	{
		// if vNormal is zero then its not semi point light, its full circle light
		CVec3 vColor, vCenter, vNormal;
		float fRadius;

		SSemiPointLight() {}
		SSemiPointLight( const CVec3 &_vColor, const CVec3 &_vCenter, const CVec3 &_vNormal, float _fRadius )
			: vColor(_vColor), vCenter(_vCenter), vNormal(_vNormal), fRadius(_fRadius) {}
	};
	struct SPointLight
	{
		CVec3 vColor, vCenter;
		float fRadius;
		
		SPointLight() {}
		SPointLight( const CVec3 _vColor, const CVec3 &_vCenter, float _fR )
			: vColor(_vColor), vCenter(_vCenter), fRadius(_fR) {}
	};
	ZDATA
	vector<SParallelLight> parallel;
	vector<SSemiPointLight> semiPoints;
	vector<SPointLight> points;
	vector<CVec3> skyDirections;
	CVec3 vAmbientColor, vUpDifColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&parallel); f.Add(3,&semiPoints); f.Add(4,&points); f.Add(5,&skyDirections); f.Add(6,&vAmbientColor); f.Add(7,&vUpDifColor); return 0; }

	void Clear() { parallel.clear(); semiPoints.clear(); points.clear(); skyDirections.clear(); }
	CVec3 GenerateSkyDir( SLightStateCalcSeed *pSeed );
	void CreateSimple( SLightStateCalcSeed *pSeed, const SGlobalIlluminationInfo &l );
	void CreateScattered( SLightStateCalcSeed *pSeed, const SGlobalIlluminationInfo &l, IGScene *pVis );
	void TraceDynamicLM( SDynamicAmbientInfo *pRes, const SSphere &bv, IGScene *pVis ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// recalcs lightmaps, has list of all lightmap textures, information about lights etc
class CLightmapTracker : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CLightmapTracker);
	enum EState
	{
		RC_START,
		RC_SKY_DEPTH,
		RC_COLOR_POINT,
		RC_COLOR_SEMI,
		RC_APPLY
	};
	struct SRecalcState
	{
		EState nState;
		int nStep;
		bool bCalcSky;
		bool bCalcColor;
	};
	ZDATA
	SGlobalIlluminationInfo globalIllumination;
	SLightStateCalcSeed ambientLightSeed;
	CLightState lightState;
	vector<SDirectionalDepthInfo> depthInfos;
	vector<CVec3> skyDirs;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&globalIllumination); f.Add(3,&ambientLightSeed); f.Add(4,&lightState); f.Add(5,&depthInfos); f.Add(6,&skyDirs); return 0; }
private:
	struct SPointLightPos
	{
		CVec3 vCenter;
		float fRadius;
		SPointLightPos() {}
		SPointLightPos( const CVec3 &_vCenter, float _fRadius ) : vCenter(_vCenter), fRadius(_fRadius) {}
		bool operator==( const SPointLightPos &p ) const { return p.vCenter == vCenter && p.fRadius == fRadius; }
	};
	struct SPointLightPosHash
	{
		int operator()( const SPointLightPos &_p ) const { const int *p = (const int*)&_p; return p[0] ^ (p[1]<<3) ^ (p[2]>>3) ^ p[3]; }
	};
	int nLights, nPassesPerCalc;
	SRecalcState rs;
	IRender *pRender;
	SBound currentBound;
	SGroupSelect groupSelect;
	SHMatrix mPrevView;
	typedef unordered_map<SPointLightPos,CObj<NGfx::CCubeTexture>, SPointLightPosHash> CPointDepthHash;
	CPointDepthHash pointDepths;
private:
	struct SLightmapTargetGeom
	{
		NGfx::CRenderContext *pRC;
		CSceneFragments *pGeom;
		CTransformStack *pTS;
		int nTargetRegister;

		SLightmapTargetGeom( CSceneFragments *_pGeom, NGfx::CRenderContext *_pRC, CTransformStack *_pTS, int _nR ) 
			: pGeom(_pGeom), pRC(_pRC), pTS(_pTS), nTargetRegister(_nR) {}
	};

	static int GetSkyTexturesNum();
	void RenderLight( SLightmapTargetGeom *pTarget, const SLightInfo &lightInfo,
		ERenderOperation op, CRenderCmdList::UParameter param1, CRenderCmdList::UParameter param2, int nStencilOp );
	void RenderCubeMapDepth( SLightmapTargetGeom *pTarget, 
		const CVec3 &vCenter, float fRadius, int nDir );
	void DownsampleCubeMapDepth( const CVec3 &vCenter, float fRadius );
	void RenderPointLightShadowed( SLightmapTargetGeom *pTarget, 
		const CVec3 &vCenter, float fRadius, const CVec3 &_vColor, NGfx::CCubeTexture *pDepth, int nDepthBias, bool bFast );
	void RenderPointNoShadows( SLightmapTargetGeom *pTarget, 
		const CVec3 &_vCenter, float fRadius, const CVec3 &_vColor );
	void RenderSkyCheck( SLightmapTargetGeom *pTarget, float fStrength, int nBuffer, bool bFast );
	void ChooseNewSkyDirection( int nBuffer, int nTarget );
	void RecalcStep( NGfx::CRenderContext *pRC, CSceneFragments *pScene, CTransformStack *pTS );
	void RecalcDepthChannel( int nBuffer, int nChannel, bool bFast );
public:
	CLightmapTracker();
	void CatchUp( NGfx::CRenderContext *pRC, IRender *_pRender, CTransformStack *pTS, CSceneFragments *pScene, bool bHasNewLightmaps, const SGroupSelect &groupSelect );
	void SetNewIllumination( const SGlobalIlluminationInfo &gl );
	const CLightState& GetLightState() const { return lightState; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif