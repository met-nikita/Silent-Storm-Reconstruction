#ifndef __Bound_H_
#define __Bound_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct SGetSelf
{
	const T& operator()( const T &a ) const { return a; }
};
struct SBoundCalcer
{
	CVec3 ptMin, ptMax;
	SBoundCalcer(): ptMin( 1e38f,1e38f,1e38f ), ptMax(-1e38f,-1e38f,-1e38f) {}
	template<class TSet,class TGetPoint>
		void LookSet( const TSet &a, TGetPoint GetPoint )
	{
		for ( TSet::const_iterator i = a.begin(); i != a.end(); ++i )
		{
			const CVec3 &p = GetPoint(*i);
			ptMin.x = Min( ptMin.x, p.x );
			ptMin.y = Min( ptMin.y, p.y );
			ptMin.z = Min( ptMin.z, p.z );
			ptMax.x = Max( ptMax.x, p.x );
			ptMax.y = Max( ptMax.y, p.y );
			ptMax.z = Max( ptMax.z, p.z );
		}
	}
	void Clear() { ptMin = CVec3( 1e38f,1e38f,1e38f ); ptMax = CVec3(-1e38f,-1e38f,-1e38f); }
	void Add( const CVec3 &p, float fRadius )
	{
		ptMin.x = Min( ptMin.x, p.x - fRadius );
		ptMin.y = Min( ptMin.y, p.y - fRadius );
		ptMin.z = Min( ptMin.z, p.z - fRadius );
		ptMax.x = Max( ptMax.x, p.x + fRadius );
		ptMax.y = Max( ptMax.y, p.y + fRadius );
		ptMax.z = Max( ptMax.z, p.z + fRadius );
	}
	void Add( const CVec3 &_p )
	{
		ptMin.x = Min( ptMin.x, _p.x );
		ptMin.y = Min( ptMin.y, _p.y );
		ptMin.z = Min( ptMin.z, _p.z );
		ptMax.x = Max( ptMax.x, _p.x );
		ptMax.y = Max( ptMax.y, _p.y );
		ptMax.z = Max( ptMax.z, _p.z );
	}
	void Add( const SBoundCalcer &bc )
	{
		ptMin.Minimize( bc.ptMin );
		ptMax.Maximize( bc.ptMax );
	}
	void Add( const SBound &bv )
	{
		ptMin.Minimize( bv.s.ptCenter - bv.ptHalfBox );
		ptMax.Maximize( bv.s.ptCenter + bv.ptHalfBox );
	}
	bool IsEmpty() const { return ptMin.x > ptMax.x; }
	void Make( SBound *pRes ) const
	{
		if ( IsEmpty() )
			pRes->BoxInit( CVec3(0,0,0), CVec3(0,0,0) ); 
		else
			pRes->BoxInit( ptMin, ptMax ); 
	}
	void Make( SSphere *pRes ) const
	{
		if ( IsEmpty() )
		{
			pRes->ptCenter = CVec3(0,0,0);
			pRes->fRadius = 0;
		}
		else
		{
			pRes->ptCenter = ( ptMax + ptMin ) * 0.5f;
			pRes->fRadius = fabs( ptMax - ptMin ) * 0.5f;
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TRes, class TSet,class TGetPoint>
inline void CalcBound( TRes *pRes, const TSet &a, TGetPoint GetPoint )
{
	SBoundCalcer b;
	b.LookSet( a, GetPoint );
	b.Make( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// should not be mixed with fpu & mmx code from StartMMXBound to StoreMMXBoundResult
#pragma warning( disable : 4799 )
inline void StartMMXBound( CVec3 *pMin, CVec3 *pMax )
{
	__asm
	{
		mov esi, pMin
		movq mm4, [esi]
		movd mm5, [esi+8]
		mov esi, pMax
		movq mm6, [esi]
		movd mm7, [esi+8]
	}
}
inline void AddMMXBoundPoint( const CVec3 *p )
{
	// Coordinates are floats, not signed integers: integer ordering reverses
	// negative values and can shrink the bound until a visible mesh is culled.
	// Keep the MMX accumulator contract used by particles, but compare in SSE
	// (without touching the x87 stack while MMX is active). Load only 12 bytes.
	__asm
	{
		mov esi, p
		movq mm0, [esi]
		movq2dq xmm0, mm0
		movq2dq xmm1, mm4
		movq2dq xmm2, mm6
		minps xmm1, xmm0
		maxps xmm2, xmm0
		movdq2q mm4, xmm1
		movdq2q mm6, xmm2

		movd xmm0, [esi+8]
		movq2dq xmm1, mm5
		movq2dq xmm2, mm7
		minss xmm1, xmm0
		maxss xmm2, xmm0
		movdq2q mm5, xmm1
		movdq2q mm7, xmm2
	}
}
inline void StoreMMXBoundResult( CVec3 *pMin, CVec3 *pMax )
{
	__asm
	{
		mov esi, pMin
		movq [esi], mm4
		movd [esi+8], mm5
		mov esi, pMax
		movq [esi], mm6
		movd [esi+8], mm7
		emms
	}
}
#pragma warning( default : 4799 )
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
