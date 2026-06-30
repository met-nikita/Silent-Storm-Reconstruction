#ifndef __AIDECISION_H_
#define __AIDECISION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// The release AI fuzzy-logic decision engine (ISign / CSign<T> / CRule<T> / CDecision<T>).
//
// Reconstructed to match the release Game.exe (verified against the decompiled
//   CDecision<CAIAction*>::GetBestAction   @ 0x00414cd0
//   CDecision<CAIAction*>::AddRule         @ 0x00416900
//   CreateRule<CAIAction*>                 @ 0x00416880
//   CreateDecision<CAIAction*>             @ 0x00416e30
//   CSign<bool>::IsTrue                    (exports/logconcrete.c)
// see reconstruction/exports/decisions.c and reconstruction/ai-architecture.md).
//
// The release replaced the predecessor's hand-coded MakeDecision priority cascades
// (e.g. CAILogicAttack::MakeDecision) with this engine. A CRule pairs a payload (a CAIAction*)
// with two sign sets: requiredSigns (ALL must hold - combined by min) and regularSigns (averaged
// in). A CDecision holds rules in priority order; GetBestAction returns the payload of the best
// rule, committing early to a strongly-true high-priority rule (the acceptance bar halves per rule).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ISign - a fuzzy condition. IsTrue() returns its 0..1 truth.
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISign: public CObjectBase
{
public:
	virtual float IsTrue() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSign<T> - a fuzzy membership function over a live variable *pVar. Fully true (1.0) at `best`,
// ramping linearly to 0 at the m_min/m_max edge on the side of *pVar, and 0 outside [m_min,m_max].
// Member order (best, m_min, m_max, pVar) and the 0x14-byte size match CSign<bool> in the release.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CSign: public ISign
{
	OBJECT_BASIC_METHODS( CSign );
public:
	T   best;          // value at which the sign is fully true
	T   m_min, m_max;  // membership-function edges
	T  *pVar;          // watched variable
	//
	CSign(): best( T() ), m_min( T() ), m_max( T() ), pVar( 0 ) {}
	CSign( T *_pVar, T _best, T _min, T _max ): best( _best ), m_min( _min ), m_max( _max ), pVar( _pVar ) {}
	//
	virtual float IsTrue()
	{
		T v = *pVar;
		if ( v == best )
			return 1.0f;
		if ( v >= m_min && v <= m_max )
		{
			T edge = ( best < v ) ? m_max : m_min;
			return 1.0f - (float)( v - best ) / (float)( edge - best );
		}
		return 0.0f;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRule<T> - a candidate decision: signs -> payload (size 0x28 for T = CAIAction*).
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CRule: public CObjectBase
{
	OBJECT_BASIC_METHODS( CRule );
public:
	vector< CPtr<ISign> > requiredSigns;       // ALL must hold (combined by min)
	vector< CPtr<ISign> > regularSigns;        // averaged in
	T action;                                  // the payload (+0x24)
	//
	CRule() : action( 0 ) {}
	CRule( T _action, const vector< CPtr<ISign> > &_required, const vector< CPtr<ISign> > &_regular )
		: requiredSigns( _required ), regularSigns( _regular ), action( _action ) {}
	//
	float IsTrue() const
	{
		float fRegular = 1.0f;
		if ( !regularSigns.empty() )
		{
			fRegular = 0.0f;
			for ( int i = 0; i < (int)regularSigns.size(); ++i )
				fRegular += regularSigns[i]->IsTrue();
			fRegular /= (float)regularSigns.size();
		}
		float fRequired = 1.0f;
		for ( int i = 0; i < (int)requiredSigns.size(); ++i )
			fRequired = Min<float>( fRequired, requiredSigns[i]->IsTrue() );
		return Min<float>( fRequired, fRegular );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDecision<T> - priority-ordered rule set (size 0x18 for T = CAIAction*).
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CDecision: public CObjectBase
{
	OBJECT_BASIC_METHODS( CDecision );
public:
	vector< CPtr< CRule<T> > > rules;          // in priority order
	//
	void AddRule( CRule<T> *pRule ) { rules.push_back( pRule ); }
	//
	T GetBestAction() const
	{
		T best = 0;
		float fBest = 0.0f, fBar = 1.0f;       // fBar halves per rule (priority decay)
		for ( int i = 0; i < (int)rules.size(); ++i )
		{
			float f = Min<float>( 1.0f, rules[i]->IsTrue() );
			if ( fBest < f )
			{
				best = rules[i]->action;
				fBest = f;
				if ( fBar * 0.5f < f )         // strongly-true high-priority rule -> commit
					return best;
			}
			fBar *= 0.5f;
		}
		return best;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// The release also exposes variadic NULL-terminated builders CreateRule<T>(action, required, regular)
// and CreateDecision<T>( rule1, ..., 0 ); we build rules/decisions directly here.
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AIDECISION_H_
