#ifndef __SECTORCTRL_H_
#define __SECTORCTRL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum EChapterSectorType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSectorCtrl: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSectorCtrl)

	vector<CVec2> sector; 
	int nMaxX;
	int nMaxY;
	int nSegmentLen;
	float fHitAccuracy;

	void ClampPoint( CVec2 *pVec );

public:
	int nTemplateID;
	int nProbability;
	int nDescrID;
	EChapterSectorType eSectorType;
	string szID;
	int nImageID;
	int nImX;
	int nImY;

public:
	CSectorCtrl( int _nMaxX = 1, int _nMaxY = 1, int _nSegmentLen = 1, const vector<CVec2> &_sector = vector<CVec2>() );
	
	const vector<CVec2> &GetPoints() const { return sector; }
	void  CreateSector( const CVec2 &pt, float fRadius, int nSegments = -1, bool bUseRadius = false );
	bool  Rebuild();
	void  Paint( CDC *pDC, const CVec2 &ptScale );
	int   RegionHitTest( const CVec2 &pt );
	bool  MovePoint( int nID, const CVec2 &pt );
	void  SetHitAccuracy( float fAccuracy ) { fHitAccuracy = fAccuracy; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void PaintCtrlPoint( CDC *pDC, const CPoint &pt, int nSpacing )
{
	pDC->MoveTo( pt.x - nSpacing, pt.y - nSpacing );
	pDC->LineTo( pt.x + nSpacing, pt.y + nSpacing );
	pDC->MoveTo( pt.x - nSpacing, pt.y + nSpacing );
	pDC->LineTo( pt.x + nSpacing, pt.y - nSpacing );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
