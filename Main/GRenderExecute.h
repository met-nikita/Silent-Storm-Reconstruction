#ifndef __GRenderExecute_H_
#define __GRenderExecute_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
class CTransformStack;
namespace NGScene
{
struct SLightInfo
{
	bool bNeedSet;
	CVec3 vGlossColor, vShadowColor, vAmbientColor, vUpDifColor;
	CVec4 vLightColor, vLightPos, vRadius;

	SLightInfo(): bNeedSet(false), vLightColor(VNULL4), vGlossColor(VNULL3), vLightPos(VNULL4), vRadius(1,1,1,1) {}
};

void Execute( IRender *pRender, NGfx::CRenderContext *pRC, const CTransformStack &ts, const CRenderCmdList &cl,
	const CSceneFragments &scene, const SLightInfo &lightInfo );
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
