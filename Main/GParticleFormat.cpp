#include "StdAfx.h"
#include "GParticleFormat.h"
#include "Bound.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticlesInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticlesInfo::CalcBound( SBound *pRes )
{
	CVec3 ptMin, ptMax;
	if ( nParticles )
	{
		float fMaxSize = 0;
		ptMin.x = ptMin.y = ptMin.z = 1e10f;
		ptMax.x = ptMax.y = ptMax.z = -1e10f;
		for ( int nP = 0; nP < nParticles; ++nP )
		{
			SParticle &part = particles[nP];
			if ( !part.pos.nKeys )
				continue;
			for ( int i = 0; i < part.pos.nKeys; ++i )
			{
				CVec3 pos = part.pos.keys[i].value;
				ptMin.Minimize( pos );
				ptMax.Maximize( pos );
			}
			for ( int i = 0; i < part.scale.nKeys; ++i )
			{
				CVec2 scale = part.scale.keys[i].value;
				fMaxSize = Max( fabs(scale.x), fMaxSize );
				fMaxSize = Max( fabs(scale.y), fMaxSize );
			}
		}
		fMaxSize *= (FP_SQRT_2 * 0.5f);
		CVec3 edge( fMaxSize, fMaxSize, fMaxSize );
		ptMin -= edge;
		ptMax += edge;
	}
	else
	{
		ptMin = VNULL3;
		ptMax = VNULL3;
	}
	pRes->BoxInit( ptMin, ptMax );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Interpolate( const CVec3 &v1, const CVec3 &v2, float fAlpha, CVec3 *pRes )
{
	*pRes = v1 * (1 - fAlpha) + v2 * fAlpha;
}
void Interpolate( const CVec2 &v1, const CVec2 &v2, float fAlpha, CVec2 *pRes )
{
	*pRes = v1 * (1 - fAlpha) + v2 * fAlpha;
}
void Interpolate( const float &v1, const float &v2, float fAlpha, float *pRes )
{
	*pRes = v1 * (1 - fAlpha) + v2 * fAlpha;
}
void Interpolate( const DWORD &v1, const DWORD &v2, float fAlpha, DWORD *pRes )
{
	DWORD b = DWORD( (v1 & 0x000000FF) * (1 - fAlpha) + (v2 & 0x000000FF) * fAlpha );
	DWORD g = DWORD( (v1 & 0x0000FF00) * (1 - fAlpha) + (v2 & 0x0000FF00) * fAlpha );
	DWORD r = DWORD( (v1 & 0x00FF0000) * (1 - fAlpha) + (v2 & 0x00FF0000) * fAlpha );
	DWORD a = DWORD( (v1 & 0xFF000000) * (1 - fAlpha) + (v2 & 0xFF000000) * fAlpha );
	*pRes = b | (g & 0x0000FF00) | (r & 0x00FF0000) | (a & 0xFF000000);
}
void Interpolate( const short &v1, const short &v2, float fAlpha, short *pRes )
{
	*pRes = v1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticlesLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileRequest* CParticlesLoader::CreateRequest()
{
	return new CFileRequest( "Effects", GetKey() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticlesLoader::RecalcValue( CFileRequest *pRequest )
{
	pValue = new CParticlesInfo;
	pValue->pData = pRequest;
	//CFileRequest &file = *pRequest;

	//file->Read( &pValue->nBytes, sizeof(int) );
	//pValue->pData.resize( pValue->nBytes );
	char *pData = (char*)pValue->pData->GetStream()->GetBufferForWrite();
	pValue->nBytes = *(int*)pData;
	pData += 4;
	//file->Read( pData, pValue->nBytes );

	char *p = pData;
	pValue->fTEnd = *( (float*)p );
	p += sizeof(float);
	pValue->fFrameRate = *( (float*)p );
	p += sizeof(float);
	pValue->nParticles = *( (int*)p );
	p += sizeof(int);
	pValue->particles = (SParticle*)p;

	for ( int nP = 0; nP < pValue->nParticles; ++nP )
	{
		SParticle &particle = pValue->particles[nP];
		particle.pos.keys = (TKey<CVec3>*)(pData + (int)particle.pos.keys);
		particle.rot.keys = (TKey<float>*)(pData + (int)particle.rot.keys);
		particle.scale.keys = (TKey<CVec2>*)(pData + (int)particle.scale.keys);
		particle.color.keys = (TKey<DWORD>*)(pData + (int)particle.color.keys);
		particle.sprite.keys = (TKey<short>*)(pData + (int)particle.sprite.keys);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*bool CParticlesLoader::NeedUpdate()
{
	bool bRes = TParent::NeedUpdate();
	return bIsFakeTexture || bRes;
}*/
/*
void CParticlesLoader::Recalc()
{
	pValue = new CParticlesInfo;
	try
	{
		CResourceFileOpener file( "Effects", GetKey() );
		
		file->Read( &pValue->nBytes, sizeof(int) );
		pValue->pData.resize( pValue->nBytes );
		char *pData = &pValue->pData[0];
		file->Read( pData, pValue->nBytes );

		char *p = pData;
		pValue->fTEnd = *( (float*)p );
		p += sizeof(float);
		pValue->fFrameRate = *( (float*)p );
		p += sizeof(float);
		pValue->nParticles = *( (int*)p );
		p += sizeof(int);
		pValue->particles = (SParticle*)p;

		for ( int nP = 0; nP < pValue->nParticles; ++nP )
		{
			SParticle &particle = pValue->particles[nP];
			particle.pos.keys = (TKey<CVec3>*)(pData + (int)particle.pos.keys);
			particle.rot.keys = (TKey<float>*)(pData + (int)particle.rot.keys);
			particle.scale.keys = (TKey<CVec2>*)(pData + (int)particle.scale.keys);
			particle.color.keys = (TKey<DWORD>*)(pData + (int)particle.color.keys);
			particle.sprite.keys = (TKey<short>*)(pData + (int)particle.sprite.keys);
		}
	}
	catch(...)
	{
		return;
	}
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02541140, CParticlesLoader );
