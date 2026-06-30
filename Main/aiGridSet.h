#ifndef __aiGridSet_H_
#define __aiGridSet_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCompareVec3
{
	bool operator()( const CVec3 &a, const CVec3 &b ) const
	{
		return a.x > b.x || ( a.x == b.x && a.y > b.y ) || ( a.x == a.y && a.y == b.y && a.z > b.z );
	}
};
struct SHeightCalcInfo
{
	list<int> hGroups;
	list<CVec3> points;
	SHeightCalcInfo() {}
	SHeightCalcInfo( int nHGroup ) { hGroups.push_back( nHGroup ); }
	SHeightCalcInfo( const vector<int> &hg ) { for ( unsigned int i = 0; i < hg.size(); ++i ) 	hGroups.push_back( hg[i] ); }
	void Merge( const SHeightCalcInfo &p )
	{
		hGroups.insert( hGroups.end(), p.hGroups.begin(), p.hGroups.end() );
		points.insert( points.end(), p.points.begin(), p.points.end() );
		hGroups.unique();
		points.unique();
	}
	bool operator==( SHeightCalcInfo &a )
	{
		hGroups.sort();
		a.hGroups.sort();
		points.sort( SCompareVec3() );
		a.points.sort( SCompareVec3() );
		return hGroups == a.hGroups && points == a.points;
	}
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &hGroups );
		f.Add( 2, &points );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
