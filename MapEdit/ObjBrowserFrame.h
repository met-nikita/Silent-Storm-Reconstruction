#ifndef __OBJBROWSERFRMAE_H_
#define __OBJBROWSERFRMAE_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
class COBView: public CScrollView
{
protected:
	virtual void PostNcDestroy() {}

public:
	void SetSizes( const CSize &size );
	virtual void OnDraw( CDC *pDC ) {};
	virtual BOOL PreCreateWindow( CREATESTRUCT &cs );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjBrowserFrame : public CFrameWnd
{
	//DECLARE_DYNCREATE(CObjBrowserFrame)
public:
	CObjBrowserFrame();           // protected constructor used by dynamic creation
	virtual ~CObjBrowserFrame();

	// Attributes
public:
	COBView m_View;

	virtual void PreSubclassWindow();

	// Implementation
protected:
	virtual void PostNcDestroy() {}

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJBROWSERFRMAE_H_