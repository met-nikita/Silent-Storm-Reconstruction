#include "StdAfx.h"
#include "GPostProcessors.h"
#include "GfxEffects.h"
#include "GfxRender.h"
#include "GRenderCore.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DoRender( NGfx::CRenderContext *pRC, const vector<IPostProcess::SObject> &render )
{
	for ( int k = 0; k < render.size(); ++k )
	{
		const IPostProcess::SObject &obj = render[k];
		SRenderGeometryInfo &info = *obj.pInfo;
		info.pVertices.Refresh();
		info.pTriLists[TLT_GEOM].Refresh();
		pRC->AddPrimitive( info.pVertices->GetValue(), info.pTriLists[TLT_GEOM]->GetValue()[ obj.nIdx ] );
	}
	pRC->Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPostColorer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPostColorer::Render( NGfx::CRenderContext *pRC, const vector<SObject> &render )
{
	pColor.Refresh();
	NGfx::SEffConstLight eff;
	eff.color = pColor->GetValue();
	pRC->SetEffect( &eff );
	pRC->SetAlphaCombine( NGfx::COMBINE_SMART_ALPHA );
	pRC->SetDepth( NGfx::DEPTH_NORMAL );
	pRC->SetStencil( NGfx::STENCIL_NONE );
	DoRender( pRC, render );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x022a2151, CPostColorer )
