#ifndef __OcTree_H_
#define __OcTree_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 node shape: the Jan03 `CVec3 ptBase` was replaced by the cached bound sphere `s`
// (PDB COcTreeNode: s@12, fSize@28, links@32, pUpLink@64). The serializer @0x167450 writes
// {2 fSize, 10..17 links, 20 pUpLink, 21 s} -- tag 1 (ptBase) is READ-ONLY legacy for v1.0 saves,
// rebuilt through the s.fRadius==0 sentinel. Dev's old tag-1 read left every node's base zeroed on
// a v1.2 load -> all static geometry collapsed onto origin-corner bound spheres -> whole-map
// frustum culling by camera accident (the "void" saves). Same template also backs the NAI octree.
template <class TFinal, int N_MIN_NODE>
class COcTreeNode: public CObjectBase
{
	typedef COcTreeNode<TFinal,N_MIN_NODE> CSelf;
	SSphere s;
	float fSize;
	CObj<TFinal> links[8];
	CPtr<TFinal> pUpLink;
protected:
	void SetUpLink( TFinal *_p ) { pUpLink = _p; }
public:
	// retail @0x68d10: cache the node bound; radius = (1+0.5)*sqrt(3)/2 * fSize (const @0x8b3810)
	void SetSize( const CVec3 &_ptBase, float _fSize )
	{
		fSize = _fSize;
		s.ptCenter.x = _ptBase.x + _fSize * 0.5f;
		s.ptCenter.y = _ptBase.y + _fSize * 0.5f;
		s.ptCenter.z = _ptBase.z + _fSize * 0.5f;
		s.fRadius = _fSize * 1.299038f;
	}
	float GetSize() const { return fSize; }
	bool Walk()
	{
		bool bRes = IsEmpty();
		for ( int i = 0; i < 8; ++i )
		{
			if ( links[i] )
			{
				if ( links[i]->Walk() )
					links[i] = 0;
				else
					bRes = false;
			}
		}
		return bRes;
	}
	virtual bool IsEmpty() { return false; }
	void GetBound( SSphere *pRes ) const { *pRes = s; }
	TFinal* GetUpLink() const { return pUpLink; }
	TFinal* GetNode( int nIdx ) { ASSERT( nIdx >= 0 && nIdx < 8 ); return links[nIdx]; }
	TFinal* GetNode( const CVec3 &ptCenter, float fRadius )
	{
		TFinal *pThis = static_cast<TFinal*>( this );
		// retail @0x167e40: descent factor 0.21650635f = 0.5*0.25*sqrt(3) (halved vs the Jan03 0.433)
		if ( fSize <= N_MIN_NODE || fRadius >= fSize * 0.21650635f )
			return pThis;
		else
		{
			int nIdx = 0;
			if ( ptCenter.x >= s.ptCenter.x )
				nIdx += 1;
			if ( ptCenter.y >= s.ptCenter.y )
				nIdx += 2;
			if ( ptCenter.z >= s.ptCenter.z )
				nIdx += 4;
			if ( !links[nIdx] )
			{
				// retail: child base = this center, minus half-size on each UNSET axis bit
				CVec3 ptNewBase( s.ptCenter );
				if ( !(nIdx & 1) )
					ptNewBase.x -= fSize * 0.5f;
				if ( !(nIdx & 2) )
					ptNewBase.y -= fSize * 0.5f;
				if ( !(nIdx & 4) )
					ptNewBase.z -= fSize * 0.5f;
				TFinal *pNewFinal = new TFinal;
				pNewFinal->SetSize( ptNewBase, fSize * 0.5f );
				links[nIdx] = pNewFinal;
				pNewFinal->SetUpLink( pThis );
			}
			return links[nIdx]->GetNode( ptCenter, fRadius );
		}
	}
	// retail @0x167450: writes {2,10..17,20,21}; tag 1 is read-only v1.0 legacy (sentinel rebuild)
	int operator&( CStructureSaver &f )
	{
		CVec3 vMin;
		if ( f.IsReading() )
			f.Add( 1, &vMin );      // missing tag -> zeroed (DataChunk memset semantics)
		f.Add( 2, &fSize );
		for ( int i = 0; i < 8; ++i )
			f.Add( 10 + i, &links[i] );
		f.Add( 20, &pUpLink );
		f.Add( 21, &s );
		if ( s.fRadius == 0 )       // v1.0 save: no tag 21 -> rebuild the bound from legacy tag 1
		{
			s.ptCenter.x = vMin.x + fSize * 0.5f;
			s.ptCenter.y = vMin.y + fSize * 0.5f;
			s.ptCenter.z = vMin.z + fSize * 0.5f;
			s.fRadius = fSize * 1.299038f;
		}
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
