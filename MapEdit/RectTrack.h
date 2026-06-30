#ifndef __RECTTRACK_H__
#define __RECTTRACK_H__

#include "TemplateView.h"

class CRectsLayer;

class CMERectTracker
{
private:
	int			nRotation;
	CPoint	ptCorners[4];
	CPoint  ptPoly[5];
	CPoint  ptPolyOld[4];
	
  int     nAngDiscrete;
	bool		bTracked;
	
	void MakePoly();
	void SetRotation( int nAngle );
	UINT GetHandleMask( ) const;
	
protected:
	CPoint ptCenter;
	int nWidth;
	int nHeight;
	const int nSpacing;
	ITemplateView *const pView;
	CRectsLayer *const pLayer;
	UINT    m_nStyle;
	
	virtual void BeginRotate( CDC *pDC ) {};
	virtual void Rotate( CDC *pDC, int nAng );
	virtual void EndRotate( CDC *pDC ) {};
	virtual void AdjustRect( CPoint *pCenter );
	bool IsTracked() const { return bTracked; }
	
public:
	CMERectTracker( ITemplateView *pView, CRectsLayer *pLayer, const CPoint &ptCenter, int nWidth, 
		int nHeight, int nRotation, int spacing, UINT nStyle = CRectTracker::solidLine );
		
	void SetStyle( UINT nStyle ) { m_nStyle = nStyle; }
	bool SetCursor( CWnd* pWnd, UINT nHitTest ) const;
		
	virtual void Draw( CDC *pDC, int nThickness, bool bGrayed );
	virtual void GetBoundsRect( CRect *pRect );
	virtual int HitTest( CPoint point );

	int  Track( CWnd* pWnd, CPoint point, bool bAllowInvert );
	int  TrackRotate( CWnd* pWnd, int angDiscrete );
	CPoint GetCeneter() const { return ptCenter; }
	void Move( const CPoint &ptDelta );
	void MoveTo( const CPoint &ptCenter );
	int  GetRotation() const { return nRotation; }
	void SetSize( int nX, int nY );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetNearestPos( int pos, int spacing )
{
  return Sign( pos ) * spacing * int(0.5 + fabs( (float)pos ) / spacing );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline COLORREF GetGray( COLORREF col )
{
	int val = 0.33333 * ( GetRValue( col) + GetBValue( col ) + GetGValue( col ) );
	return RGB( val, val, val );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RECTTRACK_H__