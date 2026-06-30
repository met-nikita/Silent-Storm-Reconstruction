#ifndef __PROPVIEW_H__
#define __PROPVIEW_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIDlg.h"
#include "PropMap.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CPropView window

class CPropView : public SECControlBar
{
protected: // create from serialization only
    
    // Attributes
public:
  
  // Operations
public:
  COIDlg m_OIDlg;

  void SetPropMap( int nTableID, const CPropMap *pPropMap );
  int  GetActiveProp( int nGroupID );
	void SetActiveProp( int nPropID );
  void UpdateProperty( int nPropID );
  
  // Implementation
  //  BOOL OnInitDialog();
  
public:
  CPropView();  
  virtual ~CPropView();
  
  // Generated message map functions
protected:
//  CModelView  *pModelView;
  
  //{{AFX_MSG(CPropView)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnChangeProp( WPARAM wParam, LPARAM lParam );
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnNcLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

#endif // __PROPVIEW_H__
