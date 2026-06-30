#ifndef __GfxShadersDescr_H_
#define __GfxShadersDescr_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <d3d9.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
// should be global variable
struct SVShader
{
	int nID;
	DWORD *pShader;
	
	SVShader( int _nID, DWORD *_pShader ): nID(_nID), pShader(_pShader) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// should be global variable
struct SRenderState
{
	D3DRENDERSTATETYPE state;
	DWORD dwVal;
};
struct STextureStageState
{
	int nStage;
	D3DTEXTURESTAGESTATETYPE state;
	DWORD dwVal;
};
struct SPShader
{
	int nID;
	DWORD *pShader, *pShader14;
	SRenderState *pShaRS;
	STextureStageState *pShaTSS;
	SRenderState *pStateRS;
	STextureStageState *pStateTSS;
	
	SPShader( 
		int _nID, DWORD *_pShader, DWORD *_pShader14,
		SRenderState *_pShaRS, STextureStageState *_pShaTSS, 
		SRenderState *_pStateRS, STextureStageState *_pStateTSS )
		:nID(_nID), pShader( _pShader ), pShader14(_pShader14),
		pShaRS(_pShaRS), pShaTSS(_pShaTSS), 
		pStateRS(_pStateRS), pStateTSS(_pStateTSS) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
