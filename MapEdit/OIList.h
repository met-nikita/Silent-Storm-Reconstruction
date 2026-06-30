#ifndef __OILIST_H_
#define __OILIST_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// COIRelListEdit dialog
#include "OIBrowEdit.h"

class CListProp;
////////////////////////////////////////////////////////////////////////////////////////////////////
class COIRelList : public COIBrowseEdit
{
public:
	void SetList( CListProp *prop );
	virtual void OnBrowse();

protected:
	CPtr<CListProp> pProp;
	//{{AFX_MSG(COIRelEdit)  
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	
  DECLARE_MESSAGE_MAP()
		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OILIST_H_#pragma once
