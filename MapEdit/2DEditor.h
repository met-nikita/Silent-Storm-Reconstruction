#ifndef __2DEDITOR_H_
#define __2DEDITOR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

////////////////////////////////////////////////////////////////////////////////////////////////////
class CMaskBrush
{
public:
	enum EStyle
	{
		BS_COS,
		BS_BELL,
	};
private:
	EStyle nStyle;
	int    nRadius;
	float  fHardness;
	CArray2D<float> mask;

	void CosineProfile();
	void BellProfile();
public:
	CMaskBrush();
	CMaskBrush( EStyle nStyle, int nRadius, float fHardness );

	void Create( EStyle nStyle, int nRadius, float fHardness );
	void Paint( CDC *pDC, COLORREF cr, int x, int y ) const;

	EStyle GetStyle() const { return nStyle; }
	int    GetRadius() const { return nRadius; }
	float  GetHardness() const { return fHardness; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class C2DEditor : public CWnd
{
protected:
	float fScale;
	COLORREF GetColor4Tile( int nTileID );
	int GetTile4Color( COLORREF cr ) const;
	void Clear();

public:
	C2DEditor();
	~C2DEditor();

	void  SetScale( float _fScale ) { fScale = _fScale; }
	float GetScale() const { return fScale; }
	void  SetImage( int nWidth, int nHeight, const CArray2D<BYTE> *pColors = 0, COLORREF cr = -2 );
	CBitmap* GetBitmap( int nZoom = 1 );
	CDC*  GetDC();
	int   GetWidth() const { return nWidth; }
	int   GetHeight() const { return nHeight; }
	DWORD GetPixel( CPoint pt ) const;
	DWORD GetPixel( int x, int y ) const { return GetPixel( CPoint( x, y ) ); }
	int   GetTileID( int x, int y) const { return GetTile4Color( GetPixel( x, y ) ); }

	void SetPixelV( int x, int y, DWORD cr );
	bool SetPixel( int x, int y, DWORD cr );
	bool SetPixel( const CPoint &pt, DWORD cr ) { return SetPixel( pt.x, pt.y, cr ); }
	void SetTileV( int x, int y, int nTileID );
	bool SetTile( int x, int y, int nTileID );
	void FillTile( int x, int y, int nTileID );
	void Fill( int x, int y, DWORD cr );
	void SetCurrentColor( COLORREF cr );

	void PaintBrush( const CMaskBrush &brush, COLORREF cr, int x, int y );
	//{{AFX_VIRTUAL(C2DEditor)
	protected:
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(C2DEditor)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	int nWidth;
	int nHeight;
	
	vector<DWORD> image;
	CBitmap bitmap;
	CDC dc;
	COLORREF currentColor;
	
	unordered_map<int, COLORREF> terrTileID2Color;
	unordered_map<COLORREF, int> color2TerrTileID;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void C2DEditor::SetTileV( int x, int y, int nTileID )
{
	SetPixelV( x, y, GetColor4Tile( nTileID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void C2DEditor::SetPixelV( int x, int y, DWORD cr )
{
	dc.SetPixelV( x, nHeight - y - 1, cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __2DEDITOR_H_
