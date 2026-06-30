#ifndef __WPOCKET_H_
#define __WPOCKET_H_
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitServer;
class CObjectServerBase;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPocket - the strategic "pocket" that carries units AND objects between maps (saveload id 0xA0523090).
// Release class, reconstructed from the matched-release decode (oracle: decomp/src/s2_pocket.h --
// MakeCopy @0x00374260, DestroyContents @0x00374490, operator& @0x00376160, the holder/place/remove
// bodies @0x00387cb0..@0x00388180).
//
// The PRE-RELEASE CWorld inlined a UNIT-ONLY pocket: a `vector<SUnitPtrHolder> pocket` member with
// CWorld::PlaceUnitInPocket / RemoveUnitFromPocket / IsUnitInPocket (wMain.h / wMain.cpp). The release
// factored that out into this class and ADDED a second, OBJECT pocket. Each entry holds an O-ref (CObj);
// only MASTER entries (units) additionally hold the M-ref (CMObj) -- plain world objects enter
// non-master (PlaceUnitInPocket bMaster=true @0x00387fb0, PlaceObjectInPocket false @0x00387fd0).
//
// This is the standalone release class (compiles + registers). Rewiring CWorld to USE it -- superseding
// the dev unit-only pocket and enabling object pocketing -- changes world serialization (chunk 44) and
// is a follow-up that must be runtime save/load-validated.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPocket: public CObjectBase
{
	OBJECT_BASIC_METHODS( CPocket );   // New / Duplicate / MakeCopy(@0x00374260) / DestroyContents(@0x00374490)
public:
	// The dual O/M smart-pointer entry (cf. the dev CWorld::SUnitPtrHolder, generalized + a master flag).
	template <class T>
	struct SSmthPtrHolder
	{
		ZDATA
		CObj<T>  pCObjHolder;    // O-ref: always held
		CMObj<T> pCMObjHolder;   // M-ref: held only for master entries (units)
		ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pCObjHolder ); f.Add( 3, &pCMObjHolder ); return 0; }
		SSmthPtrHolder() {}
		SSmthPtrHolder( T *p, bool bMaster ): pCObjHolder( p ), pCMObjHolder( bMaster ? p : 0 ) {}   // @0x003880e0 (CUnitServer) / @0x00388180 (CObjectServerBase)
		// implicit copy ctor @0x00387ff0 (CUnitServer): compiler-generated member-wise copy of pCObjHolder +
		// pCMObjHolder (each live holder AddRef'd: O-count +1 / M-count +0x100000 the CObj/CMObj copy ctors
		// carry). Left compiler-generated -- do NOT hand-write it.
	};
private:
	ZDATA
	vector< SSmthPtrHolder<CUnitServer> >        unitPocket;     // +0x0c
	vector< SSmthPtrHolder<CObjectServerBase> >  objectPocket;   // +0x18
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &unitPocket ); f.Add( 3, &objectPocket ); return 0; }
	//
	bool IsUnitInPocket( CUnitServer *pUnit ) const;             // @0x00387d10
	bool IsObjectInPocket( CObjectServerBase *pObject ) const;   // @0x00387d40
	void PlaceUnitInPocket( CUnitServer *pUnit );                // @0x00387fb0 (master)
	void PlaceObjectInPocket( CObjectServerBase *pObject );      // @0x00387fd0 (non-master)
	void RemoveUnitFromPocket( CUnitServer *pUnit );             // @0x00387e10
	void RemoveObjectFromPocket( CObjectServerBase *pObject );   // @0x00387e30
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Free templated pocket helpers -- the release factored the per-pocket logic out of CPocket into these
// (one instantiation per held type); the CPocket members delegate to them (wPocket.cpp). Mirrors the
// matched-release decode (oracle: decomp/src/s2_pocket.h, @0x00387cb0..@0x00387f00).
//
// IsSmthInPocket<T> @0x00387cb0 (CUnitServer) / @0x00387ce0 (CObjectServerBase): linear scan, true iff
// some holder's O-pointer (pCObjHolder) equals p.
template <class T>
inline bool IsSmthInPocket( T *p, const vector< CPocket::SSmthPtrHolder<T> > &v )
{
	for ( typename vector< CPocket::SSmthPtrHolder<T> >::const_iterator i = v.begin(); i != v.end(); ++i )
		if ( (*i).pCObjHolder.GetPtr() == p )
			return true;
	return false;
}
// RemoveSmthFromPocket<T> @0x00387d70 (CUnitServer) / @0x00387dc0 (CObjectServerBase): erase every entry
// whose O-pointer is p (copy-down + pop, i.e. erase-remove).
template <class T>
inline void RemoveSmthFromPocket( T *p, vector< CPocket::SSmthPtrHolder<T> > &v )
{
	for ( typename vector< CPocket::SSmthPtrHolder<T> >::iterator i = v.begin(); i != v.end(); )
	{
		if ( (*i).pCObjHolder.GetPtr() == p )
			i = v.erase( i );
		else
			++i;
	}
}
// PlaceSmthInPocket<T> @0x00387e50 (CUnitServer) / @0x00387f00 (CObjectServerBase): skip null/zombie and
// duplicates, then push_back a holder. The release gates on null AND the ZOMBIE bit (nObjData & 0x80000000);
// IsValid( p ) covers exactly null+zombie, so behaviour is preserved when delegating.
template <class T>
inline void PlaceSmthInPocket( T *p, bool bMaster, vector< CPocket::SSmthPtrHolder<T> > &v )
{
	if ( !IsValid( p ) || IsSmthInPocket( p, v ) )
		return;
	v.push_back( CPocket::SSmthPtrHolder<T>( p, bMaster ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __WPOCKET_H_
