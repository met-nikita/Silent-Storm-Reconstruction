#include "StdAfx.h"
#include "GParticleInfo.h"
#include "GParticleFormat.h"
#include "GGrass.h"
#include "Bound.h"
#include "DG.h"
#include "GfxBuffers.h"

static CVec3 NormalizedDif( const CVec3 &a, const CVec3 &b ) { CVec3 d( a - b ); Normalize(&d); return d; }
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace	NGScene
{
static float fRandomShifts[12] = { 0.02f, -0.05f, 0.07f, -0.12f, 0, 0.03f, 0.08f, -0.04f };
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTransparentTexturePlace( STransparentTexturePlace *pRes, NGfx::CTexture *pTex )
{
	NGfx::STexturePlaceInfo place;
	CObj<NGfx::CTexture> pHolder = NGfx::GetTextureContainer( pTex, &place );
	ASSERT( NGfx::HasSameContainer( NGfx::GetTransparentTextureCache(), pHolder ) );
	if ( !NGfx::HasSameContainer( NGfx::GetTransparentTextureCache(), pHolder ) )
	{
		memset( pRes, 0, sizeof(*pRes) );
		return;
	}
	float fU1 = 1.0f / place.size.x;
	float fV1 = 1.0f / place.size.y;
	float fUStart = place.place.x1 * fU1;
	float fVStart = place.place.y1 * fV1;
	float fUFinish = place.place.x2 * fU1;
	float fVFinish = place.place.y2 * fV1;
	NGfx::CalcTexCoords( &pRes->vUVs[0], fUStart, fVStart );//fVFinish );
	NGfx::CalcTexCoords( &pRes->vUVs[1], fUFinish, fVStart );//fVFinish );
	NGfx::CalcTexCoords( &pRes->vUVs[2], fUFinish, fVFinish );//fVStart );
	NGfx::CalcTexCoords( &pRes->vUVs[3], fUStart, fVFinish );//fVStart );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetGrassTexturePlace( vector<STransparentTexturePlace> *pRes, int nGrass, NGfx::CTexture *pTex )
{
	STransparentTexturePlace toSplit;
	GetTransparentTexturePlace( &toSplit, pTex );
	int nUStart = toSplit.vUVs[0].nU;
	int nUSize = toSplit.vUVs[1].nU - nUStart;
	int nVStart = toSplit.vUVs[0].nV;
	int nVSize = toSplit.vUVs[2].nV - nVStart;
	CTPoint<int> uvShift[4];
	ASSERT( nGrass > 0 );
	int nElSizeU = nUSize / nGrass;
	int nElSizeV = nVSize / nGrass;
	uvShift[0] = CTPoint<int>( 0, nElSizeV );
	uvShift[1] = CTPoint<int>( nElSizeU, nElSizeV );
	uvShift[2] = CTPoint<int>( nElSizeU, 0 );
	uvShift[3] = CTPoint<int>( 0, 0 );
	pRes->resize( nGrass * nGrass );
	for ( int k = 0; k < pRes->size(); ++k )
	{
		int nCornerU  = (k % nGrass) * nElSizeU;
		int nCornerV  = (k / nGrass) * nElSizeV;
		STransparentTexturePlace &res = (*pRes)[k];
		for ( int i = 0; i < 4; ++i )
		{
			int nU = nCornerU + uvShift[i].x;
			int nV = nVSize - ( nCornerV + uvShift[i].y );
			res.vUVs[i].nU = nUStart + nU;
			res.vUVs[i].nV = nVStart + nV;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitTexturePlaces( vector<STransparentTexturePlace> *pRes, 
	const vector<CObj<CPtrFuncBase<NGfx::CTexture> > > &tex )
{
	for ( int k = 0; k < pRes->size(); ++k )
	{
		CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTex( tex[k] );
		if ( !IsValid( pTex ) )
			continue;
		pTex.Refresh();
		GetTransparentTexturePlace( &(*pRes)[k], pTex->GetValue() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// rain build: like the above but also writes a per-entry validity flag (1=resolved, 0=skipped)
// so the rain renderer can gate each sprite without inspecting the place itself.
static void InitTexturePlaces( vector<STransparentTexturePlace> *pRes, vector<char> *pFlags,
	const vector<CObj<CPtrFuncBase<NGfx::CTexture> > > &tex )
{
	for ( int k = 0; k < pRes->size(); ++k )
	{
		CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTex( tex[k] );
		if ( !IsValid( pTex ) )
		{
			(*pFlags)[k] = 0;
			continue;
		}
		(*pFlags)[k] = 1;
		pTex.Refresh();
		GetTransparentTexturePlace( &(*pRes)[k], pTex->GetValue() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStandardParticleEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_SIN_TABLE_PERIOD = 512;
const float F_SIN_TABLE_MULT = N_SIN_TABLE_PERIOD / FP_2PI;
static float fSinTable[ N_SIN_TABLE_PERIOD * 2];
struct SSinCosTableInit
{
	SSinCosTableInit()
	{
		for ( int k = 0; k < N_SIN_TABLE_PERIOD * 2 - 1; ++k )
			fSinTable[k] = sin ( k * FP_2PI / N_SIN_TABLE_PERIOD );
	}
} sinCosTable;
inline void FastSinCos( float fAngle, float *pfSin, float *pfCos )
{
	float fA = fAngle * F_SIN_TABLE_MULT;
	int n = Float2Int( fA - 0.5f );
	float fResidual = fA - n;
	n &= N_SIN_TABLE_PERIOD - 1;
	*pfSin = fSinTable[n] + ( fSinTable[n+1] - fSinTable[n] ) * fResidual;
	n += N_SIN_TABLE_PERIOD / 4;
	*pfCos = fSinTable[n] + ( fSinTable[n+1] - fSinTable[n] ) * fResidual;
}
void CStandardParticleEffect::AddParticles( IParticleOutput *pRender )
{
	if ( !IsValid(pInfo) )
		return;
	pInfo.Refresh();
	CParticlesInfo *pParticles = pInfo->GetValue();
	if ( !pParticles )
		return;

	const SParticleOrientationInfo &or = pRender->GetOrientationInfo();
	vector<STransparentTexturePlace> texturePlaces( textures.size() );
	InitTexturePlaces( &texturePlaces, textures );

	float fCycleEnd = fEndCycle * pParticles->fFrameRate;
	for ( int nFrame = 0; nFrame < frames.size(); ++nFrame )
	{
		const SParticleFrame &frame = frames[nFrame];
		for ( int n = 0; n < pParticles->nParticles; ++n )
		{
			const SParticle &part = pParticles->particles[n];
			if ( frame.fT >= part.nTStart && frame.fT < part.nTEnd &&
				(frame.bLastCycle || part.nTStart <= fCycleEnd) )
			{
				const SParticleFrame &frame = frames[ nFrame ];

				CVec3 pos;
				float rot;	
				CVec2 scale;
				short sprite;
				unsigned int nSprite;
				DWORD dwColor;

				part.pos.GetValue( frame.fT, &pos );
				part.rot.GetValue( frame.fT, &rot );
				part.scale.GetValue( frame.fT, &scale );
				part.color.GetValue( frame.fT, &dwColor );
				part.sprite.GetValue( frame.fT, &sprite );
				nSprite = sprite;
				if ( nSprite >= textures.size() )
					continue;

				transform.RotateHVector( &pos, pos );

				// geometry
				float fSin, fCos;
				FastSinCos( rot, &fSin, &fCos );
				//float fCos = cos(rot), fSin = sin(rot);

				//CVec2 v[4];
				float fHalfScale = fScale * 0.5f;
				float fScaleX = scale.x * fHalfScale, fScaleY = scale.y * fHalfScale;
				float fPivotX = - fScaleX * pivot.x, fPivotY = -fScaleY * pivot.y;
				float fTPivotX = fCos * fPivotX - fSin * fPivotY;
				float fTPivotY = fSin * fPivotX + fCos * fPivotY;
				float fTScaleXX = fCos * fScaleX;
				float fTScaleXY = fSin * fScaleX;
				float fTScaleYX = -fSin * fScaleY;
				float fTScaleYY = fCos * fScaleY;
				//v[0].x = fPivotX - fScaleX;  v[0].y = fPivotY - fScaleY;
				//v[1].x = fPivotX + fScaleX;  v[1].y = fPivotY - fScaleY;
				//v[2].x = fPivotX + fScaleX;  v[2].y = fPivotY + fScaleY;
				//v[3].x = fPivotX - fScaleX;  v[3].y = fPivotY + fScaleY;
				float fZShiftScale = fHalfScale * ( fabs(scale.x) + fabs(scale.y) );
				int nRnd = ( n * n + nFrame ) & 7, nRndStep = n|1;

				CVec3 vPos[4];
#define ONE_VERTEX( N, xx, yy )\
				{\
				float x = fTPivotX + xx * fTScaleXX + yy * fTScaleYX;\
				float y = fTPivotY + xx * fTScaleXY + yy * fTScaleYY;\
				float fDZ = fRandomShifts[ nRnd ] * fZShiftScale;\
				vPos[N].x = pos.x + x * or.vBasic[0].x + y * or.vBasic[1].x + fDZ * or.vBasic[2].x;\
				vPos[N].y = pos.y + x * or.vBasic[0].y + y * or.vBasic[1].y + fDZ * or.vBasic[2].y;\
				vPos[N].z = pos.z + x * or.vBasic[0].z + y * or.vBasic[1].z + fDZ * or.vBasic[2].z;\
				nRnd = ( nRnd + nRndStep ) & 7;\
				}
				ONE_VERTEX( 0, -1 , -1 )
				ONE_VERTEX( 1,  1 , -1 )
				ONE_VERTEX( 2,  1 ,  1 )
				ONE_VERTEX( 3, -1 ,  1 )
#undef ONE_VERTEX
				pRender->AddParticle( vPos, dwColor, texturePlaces[nSprite], pos * or.vDepth );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrassParticleEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassParticleEffect::AddParticles( IParticleOutput *pRender )
{
	const SParticleOrientationInfo &or = pRender->GetOrientationInfo();
	vector<STransparentTexturePlace> texturePlaces( textures.size() );
	if ( nGrassSize )
	{
		ASSERT( textures.size() == 1 );
		if ( textures.empty() )
			return;
		CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTex( textures[0] );
		if ( !IsValid( pTex ) )
			return;
		pTex.Refresh();
		GetGrassTexturePlace( &texturePlaces, nGrassSize, pTex->GetValue() );
	}
	for ( int nParticle = 0; nParticle < positions.size(); ++nParticle )
	{
		CVec3 pos = positions[ nParticle ];
		float rot = 0;
		int nSprite;

		float fRandom = GetPseudoRandomForBlade( nParticle );
		float fAmp = waveAmps[ nParticle ];
		if ( fAmp > 0 )
			rot += fAmp * sin( fRandom * FP_2PI + fTEffect * (5 + fRandom * 10) );
		unsigned int nSeed = (nParticle * 713) ^ (*(int*)&pos.x);
		nSprite = (int)( nSeed % (nGrassSize * nGrassSize ) );
		//nSprite = (int)( random.Get() & 0xffffff ) % (nGrassSize * nGrassSize );

		// geometry
		float fCos = cos(rot);
		float fSin = sin(rot);

		CVec2 v[4];
		float fHalfScale = 0.5f * ( 1 - fRandom * fScaleRange );
		v[0] = fHalfScale * CVec2( - fXPivot - 1, - fYPivot - 1 );
		v[1] = fHalfScale * CVec2( - fXPivot + 1, - fYPivot - 1 );
		v[2] = fHalfScale * CVec2( - fXPivot + 1, - fYPivot + 1 );
		v[3] = fHalfScale * CVec2( - fXPivot - 1, - fYPivot + 1 );

		CVec3 dir = pos - or.vBasic[3];
		float fCamDist = dir * or.vBasic[2];
		CVec3 vPos[4];
		for ( int i = 0; i < 4; ++i )
		{
			float x = v[i].x * scale.x, y = v[i].y * scale.y;
			float xrot = fCos * x - fSin * y;
			float yrot = fSin * x + fCos * y;
			vPos[i] = dir + xrot * or.vBasic[0] + yrot * or.vBasic[1];
			vPos[i] *= (fCamDist - yrot) / fCamDist;
			vPos[i] += or.vBasic[3];
		}
		DWORD dwColor = colors[nParticle].color | 0xff000000; // set alpha to 1
		pRender->AddParticle( vPos, dwColor, texturePlaces[nSprite], pos * or.vDepth );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionParticleEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosionParticleEffect::AddParticles( IParticleOutput *pRender )
{
	if ( !IsValid(pInfo) )
		return;
	pInfo.Refresh();
	if ( !pInfo->GetValue() )
		return;
	const SParticleOrientationInfo &or = pRender->GetOrientationInfo();
	vector<STransparentTexturePlace> texturePlaces( textures.size() );
	InitTexturePlaces( &texturePlaces, textures );
	const SParticle &part = pInfo->GetValue()->particles[0];
	for ( int nParticle = 0; nParticle < positions.size(); ++nParticle )
	{
		CVec3 pos;
		float rot;	
		CVec2 scale;
		short sprite;
		DWORD dwColor;

		float fTime = fTimes[nParticle];
		part.pos.GetValue( fTime, &pos );
		part.rot.GetValue( fTime, &rot );
		part.scale.GetValue( fTime, &scale );
		part.color.GetValue( fTime, &dwColor );
		part.sprite.GetValue( fTime, &sprite );

		pos += positions[nParticle];

		// geometry
		float fCos = cos(rot);
		float fSin = sin(rot);

		CVec2 v[4];
		float fHalfScale = 0.5f;
		v[0] = fHalfScale * CVec2( - pivot.x - 1, - pivot.y - 1 );
		v[1] = fHalfScale * CVec2( - pivot.x + 1, - pivot.y - 1 );
		v[2] = fHalfScale * CVec2( - pivot.x + 1, - pivot.y + 1 );
		v[3] = fHalfScale * CVec2( - pivot.x - 1, - pivot.y + 1 );

		CVec3 vPos[4];
		for ( int i = 0; i < 4; ++i )
		{
			int nRnd = ( nParticle * nParticle + (nParticle|1) * i ) & 7;
			float x = v[i].x * scale.x, y = v[i].y * scale.y;
			float xrot = fCos * x - fSin * y;
			float yrot = fSin * x + fCos * y;
			vPos[i] = pos + xrot * or.vBasic[0] + yrot * or.vBasic[1] + fRandomShifts[ nRnd ] * or.vBasic[2];
		}
		pRender->AddParticle( vPos, dwColor, texturePlaces[sprite], pos * or.vDepth );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRainParticleEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRainParticleEffect::AddParticles( IParticleOutput *pRender )
{
	const float FP_DROP_W = 0.033f;		// streak half-width
	const float FP_DROP_H = 1.75f;		// streak length along the fall direction
	const SParticleOrientationInfo &or = pRender->GetOrientationInfo();
	vector<STransparentTexturePlace> texturePlaces( textures.size() );
	vector<char> texValid( textures.size() );
	InitTexturePlaces( &texturePlaces, &texValid, textures );
	for ( int nParticle = 0; nParticle < positions.size(); ++nParticle )
	{
		const CVec3 &pos = positions[nParticle];
		CVec3 dif = pos - or.vBasic[3];				// pos - camera
		if ( dif * or.vDepth < 0 )					// cull: keep only drops in front of the camera
		{
			const CVec3 &dir = directions[nParticle];
			// screen-perpendicular width vector = ( dif.y, -dif.x, 0 ), length FP_DROP_W
			float perpx = dif.y, perpy = -dif.x, perpz = 0;
			float fScale = FP_DROP_W / sqrt( perpx * perpx + perpy * perpy + perpz * perpz );
			perpx *= fScale; perpy *= fScale; perpz *= fScale;
			CVec3 vPos[4];
			vPos[0] = pos;
			vPos[1].x = pos.x + perpx; vPos[1].y = pos.y + perpy; vPos[1].z = pos.z + perpz;
			vPos[2].x = vPos[1].x + dir.x * FP_DROP_H;
			vPos[2].y = vPos[1].y + dir.y * FP_DROP_H;
			vPos[2].z = vPos[1].z + dir.z * FP_DROP_H;
			vPos[3].x = pos.x + dir.x * FP_DROP_H;
			vPos[3].y = pos.y + dir.y * FP_DROP_H;
			vPos[3].z = pos.z + dir.z * FP_DROP_H;
			int nSprite = faces[nParticle];			// signed-char sprite index
			if ( texValid[nSprite] )
				pRender->AddParticle( vPos, 0xffffffff, texturePlaces[nSprite], pos * or.vDepth );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGScene;
BASIC_REGISTER_CLASS(CStandardParticleEffect)
BASIC_REGISTER_CLASS(CParticleEffect)