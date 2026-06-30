#ifndef __ICONVIEW_H__
#define __ICONVIEW_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct SResTree;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjIcon window
class CObjIcon : public CWnd
{
protected:
	
	// Attributes
public:
	CBitmap m_bitmap;
  
  // Operations
public:
  CObjIcon();  
  virtual ~CObjIcon();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CObjIcon)
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CIconView window
class CIconView : public SECControlBar
{
protected:
	
	// Attributes
public:
  
  // Operations
public:
  CIconView();  
  virtual ~CIconView();

	void SetObject( const SResTree *pTree, int nObjID );

	virtual CSize CalcDynamicLayout( int nLength, DWORD dwMode );

  // Generated message map functions
protected:
	CObjIcon m_view;
  //{{AFX_MSG(CIconView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

#endif // __ICONVIEW_H__
