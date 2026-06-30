#include "StdAfx.h"
#include "GRenderFactor.h"
#include "GfxBuffers.h"
#include "GfxEffects.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\2DArray.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<NGfx::CTexture> pLightCircle, pSpecularResponse, pFogLookup, pFogTexture, pLightFalloff, pUniformBump;
NGfx::CTexture* GetUniformBump()
{
	if ( IsValid(pUniformBump) )
		return pUniformBump;
	const int N_SIZE = 4;
	pUniformBump = NGfx::MakeTexture( N_SIZE, N_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pUniformBump, 0, NGfx::INPLACE );
	for ( int x = 0; x < N_SIZE; ++x )
	{
		for ( int y = 0; y < N_SIZE; ++y )
			lock[y][x] = NGfx::SPixel8888( 128, 128, 255, 255 );
	}
	return pUniformBump;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float ConvertToFloatCoord( int n, int nSize )
{
	if ( n == 0 )
		return -1e6f;
	if ( n >= nSize - 1 )
		return 1e6f;
	return ((float)n - nSize/2) / (nSize / 6 / 2);
}
// average pixel brightness for this texture is 0.075
NGfx::CTexture* GetLightCircle()
{
	if ( IsValid( pLightCircle ) )
		return pLightCircle;
	const int N_SIZE = 256;
	pLightCircle = NGfx::MakeTexture( N_SIZE, N_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pLightCircle, 0, NGfx::INPLACE );
	for ( int x = 0; x < N_SIZE; ++x )
	{
		for ( int y = 0; y < N_SIZE; ++y )
		{
			float fX = ConvertToFloatCoord( x, N_SIZE );
			float fY = ConvertToFloatCoord( y, N_SIZE );
			float fDistance = sqrt(1+sqr(fX)+sqr(fY));
			float fFall = 4096 / (4096 + sqr(sqr(sqr(fDistance))));
			float fRes = fFall * 3 / ( 3 + sqr(fX) + sqr(fY) ) / fDistance;
			//float fRes = 1 - ( sqr(x - N_SIZE/2) + sqr(y - N_SIZE/2) ) / ((float)sqr( N_SIZE /2 ));
			fRes = Max( 0.0f, fRes );
			char cRes = Float2Int( fRes * 255 );
			lock[y][x] = NGfx::SPixel8888( cRes, cRes, cRes, cRes );
		}
	}
	return pLightCircle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGfx::CTexture* GetLightFalloff()
{
	if ( IsValid( pLightFalloff ) )
		return pLightFalloff;
	const int N_SIZE = 256;
	pLightFalloff = NGfx::MakeTexture( N_SIZE, N_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pLightFalloff, 0, NGfx::INPLACE );
	for ( int x = 0; x < N_SIZE; ++x )
	{
		for ( int y = 0; y < N_SIZE; ++y )
		{
			float fX = ConvertToFloatCoord( x, N_SIZE );
			float fY = ConvertToFloatCoord( y, N_SIZE );
			float fDistance = sqrt(1+sqr(fX)+sqr(fY));
			float fFall = 4096 / (4096 + sqr(sqr(sqr(fDistance))));
			float fRes = fFall * 1 / ( 1 + sqr(fX) + sqr(fY) );
			//float fRes = 1 - ( sqr(x - N_SIZE/2) + sqr(y - N_SIZE/2) ) / ((float)sqr( N_SIZE /2 ));
			fRes = Max( 0.0f, fRes );
			char cRes = Float2Int( fRes * 255 );
			lock[y][x] = NGfx::SPixel8888( cRes, cRes, cRes, cRes );
		}
	}
	return pLightFalloff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void RefreshSpecularResponse()
{
	if ( IsValid( pSpecularResponse ) )
		return;
	const int N_SIZE = 512;
	pSpecularResponse = NGfx::MakeTexture( N_SIZE, N_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pSpecularResponse, 0, NGfx::INPLACE );
	for ( int y = 0; y < N_SIZE; ++y )
	{
		float fRo = (y + 0.5f) / N_SIZE;
		float fPower = triple( fabs(fRo) ) * 256;
		float fResMul;
		fResMul = sqrt(sqrt( Max( fPower, 1.0f ) ));
		if ( fRo == 0 )
			fResMul = 1;
		for ( int x = 0; x < N_SIZE; ++x )
		{
			float fX = (x + 0.5f) / (N_SIZE);
			if ( fX > 0.5f )
			{
				fX = ( fX - 0.5f ) * 2;
				fX = sqrt( fX * 4 ); // fX = length(N-H)
				fX = Max( fX, (float)sqrt(0.5f) );
			}
			else
				fX = sqrt( fX ); // fX = length(N-H)
			fX = cos( 2 * asin( fX * 0.5f ) ); // fX = (N*H)
			float fRes;

			fX = fabs(fX);
			if ( fX > 0 )
				fRes = (float)(pow(fX, fPower)) * fResMul;
			else
				fRes = 0;
			fRes = Clamp( fRes * 128, 0.0f, 255.0f );
			char cRes = Float2Int( fRes );
			lock[y][x] = NGfx::SPixel8888( cRes, cRes, cRes, cRes );
			//lock[y][x] = NGfx::SPixel8888( y, x, 0, 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGfx::CTexture* GetSpecularResponse()
{
	RefreshSpecularResponse();
	return pSpecularResponse;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<NGfx::CCubeTexture> pNormalize;
const int N_CUBE_SIZE = 128;
static void NormaliseFace( NGfx::CCubeTexture *pTex, NGfx::EFace f )
{
	CVec3 n;
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pTex, f, 0, NGfx::INPLACE );
	for ( int x = 0; x < N_CUBE_SIZE; ++x )
	{
		float w = ((float)x) / (N_CUBE_SIZE - 1) * 2 - 1;
		for ( int y = 0; y < N_CUBE_SIZE; ++y )
		{
			float h = ((float)y) / (N_CUBE_SIZE - 1) * 2 - 1;
			switch( f )
			{
				case NGfx::POSITIVE_X: n = CVec3( 1.0f,    -h,    -w); break;
				case NGfx::NEGATIVE_X: n = CVec3(-1.0f,    -h,     w); break;
				case NGfx::POSITIVE_Y: n = CVec3(    w,  1.0f,     h); break;
				case NGfx::NEGATIVE_Y: n = CVec3(    w, -1.0f,    -h); break;
				case NGfx::POSITIVE_Z: n = CVec3(    w,    -h,  1.0f); break;
				case NGfx::NEGATIVE_Z: n = CVec3(   -w,    -h, -1.0f); break;
				default: ASSERT( 0 ); break;
			}
			if ( fabs2(n) > 0 )
				Normalize( &n );
			n += CVec3(1,1,1);
			n *= 127;
			lock[y][x] = NGfx::SPixel8888( Float2Int(n.x), Float2Int(n.y), Float2Int(n.z), 255 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGfx::CCubeTexture* GetNormalizeTexture()
{
	if ( IsValid( pNormalize ) )
		return pNormalize;
	pNormalize = NGfx::MakeCubeTexture( N_CUBE_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR );
	NormaliseFace( pNormalize, NGfx::POSITIVE_X );
	NormaliseFace( pNormalize, NGfx::POSITIVE_Y );
	NormaliseFace( pNormalize, NGfx::POSITIVE_Z );
	NormaliseFace( pNormalize, NGfx::NEGATIVE_X );
	NormaliseFace( pNormalize, NGfx::NEGATIVE_Y );
	NormaliseFace( pNormalize, NGfx::NEGATIVE_Z );
	return pNormalize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFogIntegrator
{
	float fHStart, fHFinish, fDH, fDH1;
public:
	CFogIntegrator( float _fS, float _fF )
	{
		fHStart = Min( _fF - 0.0001f, _fS );
		fHFinish = _fF;
		fDH = fHFinish - fHStart;
		fDH1 = 1 / fDH;
	}
	inline float Get( float fH ) const
	{
		ASSERT( fDH == fHFinish - fHStart );
		if ( fH > fHFinish )
			return fDH;
		if ( fH < fHStart )
			return -( fHStart - fH ) * 2;
		float fAlpha = ( fH - fHStart ) * fDH1;
		return fAlpha * ( 2 - fAlpha ) * fDH;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOG_LOOKUP_SIZE = 256;
NGfx::CTexture* GetFogLookupTexture( const SFogParams &_fog )
{
	static SFogParams fog;
	if ( IsValid( pFogLookup ) && fog.IsSameParams( _fog ) )
		return pFogLookup;
	if ( !IsValid( pFogLookup ) )
		pFogLookup = NGfx::MakeTexture( N_FOG_LOOKUP_SIZE, N_FOG_LOOKUP_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	fog = _fog;
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pFogLookup , 0, NGfx::INPLACE );
	CFogIntegrator fogIntegral( fog.fVapourHeightStart, fog.fHeight );
	for ( int x = 0; x < N_FOG_LOOKUP_SIZE; ++x )
	{
		float fHeight = float(x) / N_FOG_LOOKUP_SIZE * NGfx::F_FOG_HEIGHT;
		for ( int y = 0; y < N_FOG_LOOKUP_SIZE; ++y )
		{
			float fDist = float(y) / N_FOG_LOOKUP_SIZE * NGfx::F_FOG_DISTANCE;
			float fHeightDif = fog.fCameraHeight - fHeight;
			if ( fabs(fHeightDif ) < 0.0001f )
				fHeightDif = 0.0001f;
			float fMult = fDist / fabs(fHeightDif);
			float fAlpha;
			fAlpha = fabs( fogIntegral.Get( fHeight ) - fogIntegral.Get( fog.fCameraHeight ) );
			fAlpha *= fog.fDensity * fMult;
			float fBeta = 0, fSpecialDist = fDist - fog.fDistStart;
			if ( fSpecialDist > 0 )
				fBeta = 1 - pow( 2, -fSpecialDist / fog.fDist );
			fAlpha = Clamp( fAlpha, 0.0f, 1.0f );
			fBeta = Clamp( fBeta, 0.0f, 1.0f );
			CVec3 fogColor = fog.vFogColor * fBeta + fog.vWaterColor * ( fAlpha * ( 1 - fBeta ) );
			float fResAlpha = fAlpha + fBeta - fAlpha * fBeta;
			fogColor *= 255;
			fogColor.Minimize( CVec3( 255, 255, 255 ) );
			fogColor.Maximize( CVec3( 0, 0, 0 ) );
			lock[y][x] = NGfx::SPixel8888( Float2Int(fogColor.x), Float2Int(fogColor.y), Float2Int(fogColor.z), Float2Int( fResAlpha * 255 ) );
		}
	}
	return pFogLookup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOG_TEXTURE_SIZE = 128;
static void Enlarge( CArray2D<float> *pRes, const CArray2D<float> &src, float fAmpl )
{
	int nSize = src.GetXSize();
	pRes->SetSizes( nSize * 2, nSize * 2 );
	for ( int y = 0; y < nSize; ++y )
	{
		for ( int x = 0; x < nSize; ++ x )
		{
			(*pRes)[y*2][x*2] = src[y][x];
			(*pRes)[y*2+1][x*2] = ( src[y][x] + src[(y+1)&(nSize-1)][x] ) * 0.5f;
			(*pRes)[y*2][x*2+1] = ( src[y][x] + src[y][(x+1)&(nSize-1)] ) * 0.5f;
			(*pRes)[y*2+1][x*2+1] = 
				( src[y][x] + src[(y+1)&(nSize-1)][x] + 
				src[y][(x+1)&(nSize-1)] + src[(y+1)&(nSize-1)][(x+1)&(nSize-1)] ) * 0.25f;
		}
	}
	for ( int y = 0; y < nSize*2; ++y )
	{
		for ( int x = 0; x < nSize*2; ++x )
			(*pRes)[y][x] += random.GetFloat( -1, 1 ) * fAmpl;
	}
}
static void GeneratePinkNoise( CArray2D<float> *pRes, float fDecay )
{
	CArray2D<float> tmp, *pSrc, *pDst;
	pRes->Clear();
	pSrc = &tmp;
	pDst = pRes;
	float fAmpl = 1;
	tmp.SetSizes( 1, 1 );
	tmp[0][0] = 0;
	while ( pRes->GetXSize() < N_FOG_TEXTURE_SIZE )
	{
		Enlarge( pDst, *pSrc, fAmpl );
		swap( pSrc, pDst );
		fAmpl *= fDecay;
	}
}
static void Normalize( CArray2D<float> *pRes )
{
	float fM = 0, fA = 0;
	for ( int y = 0; y < pRes->GetYSize(); ++y )
	{
		for ( int x = 0; x < pRes->GetXSize(); ++x )
		{
			fM += sqr( (*pRes)[y][x] );
			fA += (*pRes)[y][x];
		}
	}
	fA /= pRes->GetXSize() * pRes->GetYSize(); 
	fM /= pRes->GetXSize() * pRes->GetYSize(); 
	for ( int y = 0; y < pRes->GetYSize(); ++y )
	{
		for ( int x = 0; x < pRes->GetXSize(); ++x )
		{
			float f = ( (*pRes)[y][x] - fA ) / fM;
			f = ( f / 6 + 0.5f ) * 256;
			(*pRes)[y][x] = Clamp( f, 0.0f, 255.0f );
		}
	}
}
static void GeneratePinkTex( CArray2D<float> *pRes, float fNoiseParam )
{
	CArray2D<float> &buf = *pRes;
	GeneratePinkNoise( &buf, fNoiseParam );
	Normalize( &buf );
}
NGfx::CTexture* GenerateFogTexture( float fNoiseParam )
{
	static float fPrevNoiseParam;
	if ( IsValid( pFogTexture ) && fPrevNoiseParam == fNoiseParam )
		return pFogTexture;
	fPrevNoiseParam = fNoiseParam;
	if ( !IsValid( pFogTexture ) )
		pFogTexture = NGfx::MakeTexture( N_FOG_TEXTURE_SIZE, N_FOG_TEXTURE_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::WRAP );
	CArray2D<float> buf1, buf2;
	GeneratePinkTex( &buf1, fNoiseParam );
	GeneratePinkTex( &buf2, fNoiseParam );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pFogTexture, 0, NGfx::INPLACE );
	for ( int x = 0; x < N_FOG_TEXTURE_SIZE; ++x )
	{
		for ( int y = 0; y < N_FOG_TEXTURE_SIZE; ++y )
			lock[y][x] = NGfx::SPixel8888( Float2Int(buf1[y][x]), Float2Int(buf2[y][x]), 255, 255 );
	}
	return pFogTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
