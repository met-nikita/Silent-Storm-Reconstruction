#ifndef __OISUBPARTSEDIT_H_
#define __OISUBPARTSEDIT_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIBrowEdit.h"

class COISubPartsEdit : public COIBrowseEdit
{
public:
	COISubPartsEdit();
	
	void SetItemID( int nID ) { nItemID = nID; }
	virtual void OnBrowse();

protected:
	int nItemID;
	//{{AFX_MSG(COISubPartsEdit)  
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	
  DECLARE_MESSAGE_MAP()
		
};

#endif // __OISUBPARTSEDIT_H_
